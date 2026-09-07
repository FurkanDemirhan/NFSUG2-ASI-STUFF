#include "BurnoutRestorations.h"
#include <windows.h>
#include <cmath>
#include <cstdio>
#include "../GameAddresses.h"
#include "Logger.h"

namespace BurnoutRestorations
{
    // GenericBurnoutScore from SPEED2.EXE at 0x00441D30
    typedef float (__cdecl* GenericBurnoutScoreFn)(void* car);
    static GenericBurnoutScoreFn GenericBurnoutScore = (GenericBurnoutScoreFn)0x00441D30;

    // FEPrintf from SPEED2.EXE at 0x00537B80
    typedef int (*FEPrintfFn)(const char* pkg_name, int obj_hash, const char* fmt, ...);
    static FEPrintfFn FEPrintf = (FEPrintfFn)0x00537B80;

    static BurnoutState s_State = {
        false, // isBurnoutActive
        0.0f,  // currentYawRate
        0.0f,  // accumulatedCWAngle
        0.0f,  // accumulatedCCWAngle
        0.0f,  // totalBurnoutDistance
        0.0f,  // sessionScore
        1,     // comboMultiplier
        TRICK_NONE,
        0.0f   // timeSinceLastTrick
    };

    static float s_PreviousYawRate = 0.0f;
    static float s_YawRateSignTimer = 0.0f;
    static float s_MessageDisplayTimer = 0.0f;
    static char s_LastTrickText[128] = { 0 };

    const BurnoutState& GetState()
    {
        return s_State;
    }

    static void AwardTrick(eBurnoutTrick trick, const char* trickName, int basePoints)
    {
        int awardedPoints = basePoints * s_State.comboMultiplier;
        s_State.sessionScore += awardedPoints;
        s_State.lastTrick = trick;
        s_State.timeSinceLastTrick = 0.0f;

        if (s_State.comboMultiplier < 5)
        {
            s_State.comboMultiplier++;
        }

        snprintf(s_LastTrickText, sizeof(s_LastTrickText), "%s +%d (x%d)", trickName, awardedPoints, s_State.comboMultiplier);
        s_MessageDisplayTimer = 2.5f;

        Logger::Log("[Burnout] Trick Awarded: %s (Total: %.0f pts, Dist: %.1fm)",
            s_LastTrickText, s_State.sessionScore, s_State.totalBurnoutDistance);

        // Notify HUD
        uint32_t msgHash = GameAddresses::bStringHash("RaceOverMessage");
        uint32_t leaderHash = GameAddresses::bStringHash("LeaderText_1");

        // Try HUD_CarShow.fng then HUD_SingleRace.fng
        FEPrintf("HUD_CarShow.fng", leaderHash, "%s", s_LastTrickText);
        FEPrintf("HUD_SingleRace.fng", msgHash, "%s", s_LastTrickText);
    }

    void Update(float dt)
    {
        // Only run when in Gameplay flow (TheGameFlowManager == 6)
        uint32_t flowManager = *(uint32_t*)GameAddresses::TheGameFlowManager;
        if (flowManager != 6)
        {
            s_State.isBurnoutActive = false;
            return;
        }

        uint8_t isBurnoutMode = *(uint8_t*)0x0089E7E2;
        if (!isBurnoutMode)
        {
            return;
        }

        uintptr_t playerArray = *(uintptr_t*)GameAddresses::PlayersByNumber;
        if (!playerArray)
        {
            return;
        }

        uintptr_t player = *(uintptr_t*)playerArray;
        if (!player)
        {
            return;
        }

        // Get Player Car pointer: [player + 4]
        void* car = *(void**)(player + 4);
        if (!car)
        {
            return;
        }

        // Call GenericBurnoutScore to check tire slip & RPM criteria
        float score = GenericBurnoutScore(car);

        // Get SimVehicle at [car + 0x1C]
        uintptr_t simVehicle = *(uintptr_t*)((uintptr_t)car + 0x1C);
        if (!simVehicle)
        {
            return;
        }

        // Speed vector at [simVehicle + 0x42C]
        float vx = *(float*)(simVehicle + 0x42C);
        float vy = *(float*)(simVehicle + 0x430);
        float vz = *(float*)(simVehicle + 0x434);
        float speed = std::sqrt(vx * vx + vy * vy + vz * vz);

        // Angular yaw rate at [simVehicle + 0x43C]
        float yawRate = *(float*)(simVehicle + 0x43C);
        s_State.currentYawRate = yawRate;

        if (score > 0.0f)
        {
            s_State.isBurnoutActive = true;
            s_State.totalBurnoutDistance += speed * dt;
            s_State.sessionScore += score * dt * 50.0f;

            // Clockwise Donut detection (yawRate < -1.4f)
            if (yawRate < -1.4f && speed < 16.0f)
            {
                s_State.accumulatedCWAngle += std::fabs(yawRate) * dt;
                if (s_State.accumulatedCWAngle >= 6.2831853f) // 2 * PI
                {
                    s_State.accumulatedCWAngle = 0.0f;
                    if (s_State.lastTrick == TRICK_CCW_DONUT && s_State.timeSinceLastTrick < 4.5f)
                    {
                        AwardTrick(TRICK_FIGURE_8, "FIGURE 8", 2000);
                    }
                    else
                    {
                        AwardTrick(TRICK_CW_DONUT, "CW DONUT", 500);
                    }
                }
            }
            else
            {
                s_State.accumulatedCWAngle = std::max(0.0f, s_State.accumulatedCWAngle - dt * 2.0f);
            }

            // Counter-Clockwise Donut detection (yawRate > 1.4f)
            if (yawRate > 1.4f && speed < 16.0f)
            {
                s_State.accumulatedCCWAngle += std::fabs(yawRate) * dt;
                if (s_State.accumulatedCCWAngle >= 6.2831853f) // 2 * PI
                {
                    s_State.accumulatedCCWAngle = 0.0f;
                    if (s_State.lastTrick == TRICK_CW_DONUT && s_State.timeSinceLastTrick < 4.5f)
                    {
                        AwardTrick(TRICK_FIGURE_8, "FIGURE 8", 2000);
                    }
                    else
                    {
                        AwardTrick(TRICK_CCW_DONUT, "CCW DONUT", 500);
                    }
                }
            }
            else
            {
                s_State.accumulatedCCWAngle = std::max(0.0f, s_State.accumulatedCCWAngle - dt * 2.0f);
            }

            // S-Curve & S-Mirror detection: rapid yaw sign transition under speed
            if ((s_PreviousYawRate > 1.2f && yawRate < -1.2f) || (s_PreviousYawRate < -1.2f && yawRate > 1.2f))
            {
                if (speed >= 8.0f && s_YawRateSignTimer < 1.0f)
                {
                    if (yawRate < 0.0f)
                    {
                        AwardTrick(TRICK_S_CURVE, "S CURVE", 750);
                    }
                    else
                    {
                        AwardTrick(TRICK_S_MIRROR, "S MIRROR", 1000);
                    }
                }
                s_YawRateSignTimer = 0.0f;
            }
            else
            {
                s_YawRateSignTimer += dt;
            }

            s_PreviousYawRate = yawRate;
        }
        else
        {
            s_State.isBurnoutActive = false;
            s_State.accumulatedCWAngle = 0.0f;
            s_State.accumulatedCCWAngle = 0.0f;

            // Reset combo multiplier if player stops drifting/burnout for > 3.5 seconds
            s_State.timeSinceLastTrick += dt;
            if (s_State.timeSinceLastTrick > 3.5f)
            {
                s_State.comboMultiplier = 1;
            }
        }

        // Keep displaying active message
        if (s_MessageDisplayTimer > 0.0f)
        {
            s_MessageDisplayTimer -= dt;
            if (s_MessageDisplayTimer <= 0.0f)
            {
                uint32_t msgHash = GameAddresses::bStringHash("RaceOverMessage");
                uint32_t leaderHash = GameAddresses::bStringHash("LeaderText_1");
                FEPrintf("HUD_CarShow.fng", leaderHash, "");
                FEPrintf("HUD_SingleRace.fng", msgHash, "");
            }
        }
    }

    void Init()
    {
        Logger::Log("[Burnout] Initializing Burnout trick judging & scoring engine...");
    }
}
