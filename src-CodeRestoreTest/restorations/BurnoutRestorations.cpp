#include "BurnoutRestorations.h"
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "../GameAddresses.h"
#include "Logger.h"

namespace BurnoutRestorations
{
    // GenericBurnoutScore from SPEED2.EXE at 0x00441D30
    // Expects SimVehicle* (checks wheel slip ratios at [ecx+0x3B4..0x3CC] and forward velocity at [ecx+0x42C])
    typedef float (__cdecl* GenericBurnoutScoreFn)(void* simVehicle);
    static GenericBurnoutScoreFn GenericBurnoutScore = (GenericBurnoutScoreFn)0x00441D30;

    // FEng API functions in SPEED2.EXE
    typedef void* (*FEngFindObjectFn)(const char* pkg_name, uint32_t obj_hash);
    static FEngFindObjectFn FEngFindObject = (FEngFindObjectFn)0x005379C0;

    typedef void (*FEngSetVisibleFn)(void* obj);
    static FEngSetVisibleFn FEngSetVisibleObj = (FEngSetVisibleFn)0x0050CA00;

    typedef void (*FEngSetInvisibleFn)(void* obj);
    static FEngSetInvisibleFn FEngSetInvisibleObj = (FEngSetInvisibleFn)0x0050CA50;

    typedef int (*FEngSetScriptFn)(const char* pkg_name, uint32_t obj_hash, const char* script_name, int unk);
    static FEngSetScriptFn FEngSetScript = (FEngSetScriptFn)0x00537C00;

    typedef int (*FEPrintfFn)(const char* pkg_name, uint32_t obj_hash, const char* fmt, ...);
    static FEPrintfFn FEPrintf = (FEPrintfFn)0x00537B80;

    // Style Points Accumulator TriggerMoment from SPEED2.EXE at 0x0056ED10
    // __thiscall: ecx = pAccumulator, args = (int momentType, float score, float multiplier)
    typedef void (__thiscall* TriggerMomentFn)(void* pAccumulator, int momentType, float score, float multiplier);
    static TriggerMomentFn TriggerMoment = (TriggerMomentFn)0x0056ED10;

    // Multiplier curve from PS2 Demo / GameCube builds
    static const float k_Multipliers[] = { 1.0f, 1.4f, 1.7f, 2.0f, 2.5f };
    static const int k_MaxMultiplierLevel = 4;

    // FNG Object Hashes
    static const uint32_t k_SplitTimeTextHash   = 0xF59D909F; // bStringHashUpper("SplitTimeText")
    static const uint32_t k_RaceOverMessageHash = 0xA7806DDB; // bStringHashUpper("RaceOverMessage")

    static const char* const k_HUDPackages[] = {
        "HUD_CarShow.fng",
        "HUD_SingleRace.fng",
        "HUD_DriftRace.fng"
    };

    static BurnoutState s_State = {
        false, // isBurnoutActive
        0.0f,  // currentYawRate
        0.0f,  // accumulatedCWAngle
        0.0f,  // accumulatedCCWAngle
        0.0f,  // totalBurnoutDistance
        0.0f,  // sessionScore
        1.0f,  // comboMultiplier
        0,     // comboLevel
        TRICK_NONE,
        0.0f   // timeSinceLastTrick
    };

    static float s_PreviousYawRate = 0.0f;
    static float s_YawRateSignTimer = 0.0f;
    static float s_MessageDisplayTimer = 0.0f;
    static float s_BurnoutMomentTimer = 0.0f;
    static bool  s_WasReversing = false;
    static float s_ReverseTimer = 0.0f;
    static char  s_LastTrickText[128] = { 0 };

    const BurnoutState& GetState()
    {
        return s_State;
    }

    static void ShowTrickOnHUD(const char* text)
    {
        for (const char* pkg : k_HUDPackages)
        {
            // Primary animated center banner: SplitTimeText
            void* objSplit = FEngFindObject(pkg, k_SplitTimeTextHash);
            if (objSplit)
            {
                FEngSetVisibleObj(objSplit);
                FEPrintf(pkg, k_SplitTimeTextHash, "%s", text);
                FEngSetScript(pkg, k_SplitTimeTextHash, "ZoomInGreen", 1);
            }

            // Secondary / fallback center message: RaceOverMessage
            void* objMsg = FEngFindObject(pkg, k_RaceOverMessageHash);
            if (objMsg)
            {
                FEngSetVisibleObj(objMsg);
                FEPrintf(pkg, k_RaceOverMessageHash, "%s", text);
            }
        }
    }

    static void ClearTrickFromHUD()
    {
        for (const char* pkg : k_HUDPackages)
        {
            void* objSplit = FEngFindObject(pkg, k_SplitTimeTextHash);
            if (objSplit)
            {
                FEPrintf(pkg, k_SplitTimeTextHash, "");
                FEngSetInvisibleObj(objSplit);
            }

            void* objMsg = FEngFindObject(pkg, k_RaceOverMessageHash);
            if (objMsg)
            {
                FEPrintf(pkg, k_RaceOverMessageHash, "");
                FEngSetInvisibleObj(objMsg);
            }
        }
    }

    static void AwardTrick(eBurnoutTrick trick, const char* trickName, int basePoints, uintptr_t simVehicle)
    {
        int awardedPoints = static_cast<int>(basePoints * s_State.comboMultiplier);
        s_State.sessionScore += awardedPoints;
        s_State.lastTrick = trick;
        s_State.timeSinceLastTrick = 0.0f;

        // Advance combo multiplier level
        if (s_State.comboLevel < k_MaxMultiplierLevel)
        {
            s_State.comboLevel++;
            s_State.comboMultiplier = k_Multipliers[s_State.comboLevel];
        }

        snprintf(s_LastTrickText, sizeof(s_LastTrickText), "%s +%d (%.1fx)",
            trickName, awardedPoints, s_State.comboMultiplier);
        s_MessageDisplayTimer = 3.0f;

        Logger::Log("[Burnout] Trick Awarded: %s (Total: %.0f pts, Dist: %.1fm)",
            s_LastTrickText, s_State.sessionScore, s_State.totalBurnoutDistance);

        // Display prominently on screen
        ShowTrickOnHUD(s_LastTrickText);

        // Trigger native StyleMoment on SimVehicle->m_pStylePointsAccumulator ([simVehicle + 0x1D4])
        if (simVehicle)
        {
            void* pAccumulator = *(void**)(simVehicle + 0x1D4);
            if (pAccumulator)
            {
                // Moment type indices matching PS2 Demo / GameCube builds:
                // 30 = STYLE_DONUT_CW
                // 31 = STYLE_DONUT_CCW
                // 32 = STYLE_SCURVE
                // 33 = STYLE_SCURVE_MIRROR
                // 34 = FIGURE 8 / STYLE_BURNOUT
                // 20 = STYLE_360
                // 25 = STYLE_JTURN
                int momentType = 30;
                switch (trick)
                {
                    case TRICK_CW_DONUT:  momentType = 30; break;
                    case TRICK_CCW_DONUT: momentType = 31; break;
                    case TRICK_S_CURVE:   momentType = 32; break;
                    case TRICK_S_MIRROR:  momentType = 33; break;
                    case TRICK_FIGURE_8:  momentType = 34; break;
                    case TRICK_360_SPIN:  momentType = 20; break;
                    case TRICK_J_TURN:    momentType = 25; break;
                    default:              momentType = 26; break; // STYLE_BURNOUT
                }

                TriggerMoment(pAccumulator, momentType, (float)awardedPoints, s_State.comboMultiplier);
            }
        }
    }

    void Update(float dt)
    {
        // Only run when in Gameplay flow (TheGameFlowManager == 6)
        uint32_t flowManager = *(uint32_t*)GameAddresses::TheGameFlowManager;
        if (flowManager != 6)
        {
            if (s_State.isBurnoutActive || s_State.sessionScore > 0.0f)
            {
                s_State.isBurnoutActive = false;
                s_State.sessionScore = 0.0f;
                s_State.totalBurnoutDistance = 0.0f;
                s_State.comboLevel = 0;
                s_State.comboMultiplier = k_Multipliers[0];
                s_State.accumulatedCWAngle = 0.0f;
                s_State.accumulatedCCWAngle = 0.0f;
                s_MessageDisplayTimer = 0.0f;
                ClearTrickFromHUD();
            }
            return;
        }

        uint8_t isBurnoutMode = *(uint8_t*)0x0089E7E2;
        uint32_t qrMode = *(uint32_t*)0x0083AAB4;
        if (!isBurnoutMode && qrMode != 11)
        {
            return;
        }

        // 0x008900AC is Player* PlayersByNumber[4] (Direct pointer to Player 0)
        void* player = *(void**)GameAddresses::PlayersByNumber;
        if (!player)
        {
            return;
        }

        // Get Player 0 Car pointer: [player + 4]
        void* car = *(void**)((uintptr_t)player + 4);
        if (!car)
        {
            return;
        }

        // Get SimVehicle at [car + 0x1C]
        uintptr_t simVehicle = *(uintptr_t*)((uintptr_t)car + 0x1C);
        if (!simVehicle)
        {
            return;
        }

        // Vehicle velocity vector at [simVehicle + 0x42C..0x434]
        float vx = *(float*)(simVehicle + 0x42C);
        float vy = *(float*)(simVehicle + 0x430);
        float vz = *(float*)(simVehicle + 0x434);
        float speed = std::sqrt(vx * vx + vy * vy + vz * vz);

        // Angular yaw rate at [simVehicle + 0x43C]
        float yawRate = *(float*)(simVehicle + 0x43C);
        s_State.currentYawRate = yawRate;

        // Call GenericBurnoutScore to check wheel slip ratios & tire spin
        float burnoutScore = GenericBurnoutScore((void*)simVehicle);

        // -------------------------------------------------------------------------
        // 1. Continuous Burnout / Tire Smoke Scoring
        // -------------------------------------------------------------------------
        if (burnoutScore > 0.0f)
        {
            s_State.isBurnoutActive = true;
            s_State.totalBurnoutDistance += speed * dt;
            s_State.sessionScore += burnoutScore * dt * 50.0f;

            // Periodically trigger STYLE_BURNOUT moment (slot 26)
            s_BurnoutMomentTimer += dt;
            if (s_BurnoutMomentTimer >= 1.0f)
            {
                s_BurnoutMomentTimer = 0.0f;
                void* pAccumulator = *(void**)(simVehicle + 0x1D4);
                if (pAccumulator)
                {
                    TriggerMoment(pAccumulator, 26, 50.0f * s_State.comboMultiplier, s_State.comboMultiplier);
                }
            }
        }
        else
        {
            s_State.isBurnoutActive = false;
            s_BurnoutMomentTimer = 0.0f;
        }

        // -------------------------------------------------------------------------
        // 2. Donut & Figure 8 Trick Detection (Low speed, high continuous rotation)
        // -------------------------------------------------------------------------
        if (speed < 18.0f)
        {
            // Clockwise Donut (yawRate < -1.1 rad/s)
            if (yawRate < -1.1f)
            {
                s_State.accumulatedCWAngle += std::fabs(yawRate) * dt;
                if (s_State.accumulatedCWAngle >= 6.2831853f) // 360 degrees (2 * PI)
                {
                    s_State.accumulatedCWAngle = 0.0f;
                    if (s_State.lastTrick == TRICK_CCW_DONUT && s_State.timeSinceLastTrick < 5.0f)
                    {
                        AwardTrick(TRICK_FIGURE_8, "FIGURE 8", 2000, simVehicle);
                    }
                    else
                    {
                        AwardTrick(TRICK_CW_DONUT, "CW DONUT", 500, simVehicle);
                    }
                }
            }
            else
            {
                s_State.accumulatedCWAngle = (std::max)(0.0f, s_State.accumulatedCWAngle - dt * 2.0f);
            }

            // Counter-Clockwise Donut (yawRate > 1.1 rad/s)
            if (yawRate > 1.1f)
            {
                s_State.accumulatedCCWAngle += std::fabs(yawRate) * dt;
                if (s_State.accumulatedCCWAngle >= 6.2831853f) // 360 degrees (2 * PI)
                {
                    s_State.accumulatedCCWAngle = 0.0f;
                    if (s_State.lastTrick == TRICK_CW_DONUT && s_State.timeSinceLastTrick < 5.0f)
                    {
                        AwardTrick(TRICK_FIGURE_8, "FIGURE 8", 2000, simVehicle);
                    }
                    else
                    {
                        AwardTrick(TRICK_CCW_DONUT, "CCW DONUT", 500, simVehicle);
                    }
                }
            }
            else
            {
                s_State.accumulatedCCWAngle = (std::max)(0.0f, s_State.accumulatedCCWAngle - dt * 2.0f);
            }
        }
        else
        {
            // High speed: decay accumulated donut angles
            s_State.accumulatedCWAngle = (std::max)(0.0f, s_State.accumulatedCWAngle - dt * 3.0f);
            s_State.accumulatedCCWAngle = (std::max)(0.0f, s_State.accumulatedCCWAngle - dt * 3.0f);

            // ---------------------------------------------------------------------
            // 3. 360 Spin Trick Detection (High speed full rotation)
            // ---------------------------------------------------------------------
            if (std::fabs(yawRate) > 1.8f)
            {
                static float s_360Angle = 0.0f;
                s_360Angle += std::fabs(yawRate) * dt;
                if (s_360Angle >= 6.2831853f)
                {
                    s_360Angle = 0.0f;
                    AwardTrick(TRICK_360_SPIN, "360 SPIN", 1500, simVehicle);
                }
            }
        }

        // -------------------------------------------------------------------------
        // 4. S-Curve & S-Mirror Detection (Rapid yaw transition under drifting speed)
        // -------------------------------------------------------------------------
        if (speed >= 6.0f)
        {
            if ((s_PreviousYawRate > 1.0f && yawRate < -1.0f) || (s_PreviousYawRate < -1.0f && yawRate > 1.0f))
            {
                if (s_YawRateSignTimer < 1.2f)
                {
                    if (yawRate < 0.0f)
                    {
                        AwardTrick(TRICK_S_CURVE, "S CURVE", 750, simVehicle);
                    }
                    else
                    {
                        AwardTrick(TRICK_S_MIRROR, "S MIRROR", 1000, simVehicle);
                    }
                }
                s_YawRateSignTimer = 0.0f;
            }
            else
            {
                s_YawRateSignTimer += dt;
            }
        }

        s_PreviousYawRate = yawRate;

        // -------------------------------------------------------------------------
        // 5. J-Turn Detection (Reverse motion into forward 180 spin)
        // -------------------------------------------------------------------------
        // Local vehicle forward vector projection or reverse velocity check
        if (vx < -4.0f)
        {
            s_WasReversing = true;
            s_ReverseTimer = 0.0f;
        }
        else if (s_WasReversing)
        {
            s_ReverseTimer += dt;
            if (vx > 3.0f && std::fabs(yawRate) > 1.2f)
            {
                s_WasReversing = false;
                AwardTrick(TRICK_J_TURN, "J TURN", 1000, simVehicle);
            }
            else if (s_ReverseTimer > 2.5f)
            {
                s_WasReversing = false;
            }
        }

        // -------------------------------------------------------------------------
        // 6. Combo Multiplier Decay Timer
        // -------------------------------------------------------------------------
        s_State.timeSinceLastTrick += dt;
        if (s_State.timeSinceLastTrick > 5.0f)
        {
            s_State.comboLevel = 0;
            s_State.comboMultiplier = k_Multipliers[0];
        }

        // -------------------------------------------------------------------------
        // 7. On-Screen Message Display Timer
        // -------------------------------------------------------------------------
        if (s_MessageDisplayTimer > 0.0f)
        {
            s_MessageDisplayTimer -= dt;
            if (s_MessageDisplayTimer <= 0.0f)
            {
                ClearTrickFromHUD();
            }
        }
    }

    void Init()
    {
        Logger::Log("[Burnout] Initializing Burnout trick judging & scoring engine...");
    }
}
