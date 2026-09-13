#include "DemoCareerRestorations.h"
#include "../Config.h"
#include "../Logger.h"
#include "../includes/injector/injector.hpp"
#include <windows.h>

// PS2 Demo career behavior restoration (SLUS_291.18 -> SPEED2.EXE v1.2 US).
//
// The PS2 Demo's RandomEncounterManager (the career free-roam outrun engagement system)
// ran a 6-state machine. Its enum-name debug table lives in the demo ELF .data at
// 0x3B8DB0 (verified by pointer scan):
//
//   0x37A088 "eRANDOM_ENCOUNTER_MANAGER_STATE_NONE"
//   0x37A0B0 "eRANDOM_ENCOUNTER_MANAGER_STATE_POLL_CAREER"
//   0x37A0E0 "eRANDOM_ENCOUNTER_MANAGER_STATE_LOADING_OPPONENTS"
//   0x37A118 "eRANDOM_ENCOUNTER_MANAGER_STATE_TRYING_TO_ENGAGE"
//   0x37A150 "eRANDOM_ENCOUNTER_MANAGER_STATE_RACING"
//   0x37A178 "eRANDOM_ENCOUNTER_MANAGER_STATE_DEACTIVATE_OPPONENT"   <- 6th state
//
// The retail PC build only kept the first five states (debug strings present in
// SPEED2.EXE, "DEACTIVATE_OPPONENT" absent): after an outrun encounter finished, the
// demo transitioned through DEACTIVATE_OPPONENT to return the engaged opponent car to
// the world pool, while retail tears the race down without that cleanup step.
//
// Verified retail-side structures used to re-implement the behavior:
//   - RandomEncounterManager::TryToEngagePlayer (0x413030) iterates the Player* array
//     at 0x8900B4 (count at 0x8900BC) and stores the polled encounter opponent in
//     Player + 0x250:  mov dword ptr [edi + 0x250], esi
//   - A promoted opponent is a real racer: Car + 0x1C holds its RacingCar* while the
//     encounter race is live (TheRace at 0x890118).
//   - Deactivation = clearing Car + 0x550 (mIsActive), exactly what the engine's own
//     traffic despawn path does via Car::SetRenderingEnabled (0x5ED5C0, called from
//     TrafficController at 0x42C92A / 0x4099FD):  mov byte ptr [ecx + 0x550], al
//
// Update() watches TheRace transitions in the main-loop tick: when a race starts, the
// players' current encounter candidates (that were promoted to racers) are captured;
// when the race ends, each captured opponent that is still active in the world is
// deactivated - the demo's DEACTIVATE_OPPONENT state, as an INI-gated restoration.

namespace
{
    constexpr uintptr_t AddrTheRace           = 0x890118; // TheRace (Race*)
    constexpr uintptr_t AddrWorld             = 0x890080; // TheWorld
    constexpr uintptr_t AddrPlayersArray      = 0x8900B4; // Player* array polled by TryToEngagePlayer
    constexpr uintptr_t AddrPlayersArrayCount = 0x8900BC;

    constexpr uintptr_t PlayerEncounterCarOffset = 0x250;  // Player + 0x250 = Car* (TryToEngagePlayer)
    constexpr uintptr_t CarRacingCarOffset       = 0x1C;   // Car + 0x1C = RacingCar* while racing
    constexpr uintptr_t CarIsActiveOffset        = 0x550;  // Car + 0x550 = mIsActive (SetRenderingEnabled / despawn path)

    constexpr int MaxPlayers   = 8;
    constexpr int MaxWorldCars = 64;

    bool      s_RaceWasActive = false;
    uintptr_t s_CapturedOpponents[MaxPlayers] = { 0 };
    int       s_CapturedCount = 0;

    bool IsPointerPlausible(uintptr_t addr)
    {
        return addr >= 0x00400000 && addr < 0x7FFFFFFF;
    }

    bool IsCarInWorld(uintptr_t car)
    {
        uintptr_t world = *(uintptr_t*)AddrWorld;
        if (!IsPointerPlausible(world))
            return false;

        int totalCars = *(int*)(world + 0x10);
        if (totalCars <= 0 || totalCars > MaxWorldCars)
            return false;

        for (int i = 0; i < totalCars; i++)
        {
            if (*(uintptr_t*)(world + 0x1C + i * 4) == car)
                return true;
        }
        return false;
    }

    void CaptureEncounterOpponents()
    {
        s_CapturedCount = 0;

        int playerCount = *(int*)AddrPlayersArrayCount;
        if (playerCount <= 0 || playerCount > MaxPlayers)
            return;

        for (int i = 0; i < playerCount; i++)
        {
            uintptr_t player = *(uintptr_t*)(AddrPlayersArray + i * 4);
            if (!IsPointerPlausible(player))
                continue;

            uintptr_t car = *(uintptr_t*)(player + PlayerEncounterCarOffset);
            if (!IsPointerPlausible(car) || car == *(uintptr_t*)(player + 4))
                continue;

            // Only cars actually promoted to racers by the encounter system
            if (*(uintptr_t*)(car + CarRacingCarOffset) == 0)
                continue;

            if (s_CapturedCount < MaxPlayers)
                s_CapturedOpponents[s_CapturedCount++] = car;

            Logger::Log("[DemoCareer] Encounter race started: tracking opponent car 0x%08X (player %d)", (unsigned int)car, i);
        }
    }

    void DeactivateOpponents()
    {
        for (int i = 0; i < s_CapturedCount; i++)
        {
            uintptr_t car = s_CapturedOpponents[i];
            if (!IsPointerPlausible(car) || !IsCarInWorld(car))
                continue;

            if (*(uint8_t*)(car + CarIsActiveOffset) != 0)
            {
                *(uint8_t*)(car + CarIsActiveOffset) = 0; // mIsActive = 0, engine pool reclaims it
                Logger::Log("[DemoCareer] Encounter race ended: deactivated opponent car 0x%08X (DEACTIVATE_OPPONENT)", (unsigned int)car);
            }
        }
        s_CapturedCount = 0;
    }
}

namespace DemoCareerRestorations
{
    void Install()
    {
        s_RaceWasActive = false;
        s_CapturedCount = 0;
    }

    void Update()
    {
        uintptr_t race = *(uintptr_t*)AddrTheRace;

        if (race && IsPointerPlausible(race))
        {
            if (!s_RaceWasActive)
            {
                s_RaceWasActive = true;
                CaptureEncounterOpponents();
            }
        }
        else if (s_RaceWasActive)
        {
            s_RaceWasActive = false;
            DeactivateOpponents();
        }
    }
}
