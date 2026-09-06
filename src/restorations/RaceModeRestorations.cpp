#include "RaceModeRestorations.h"
#include "../GameAddresses.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"

namespace RaceModeRestorations
{
    void Install()
    {
        // 1. Restore Outrun mode selection in Quick Race menu
        injector::MakeNOP(0x4B2F3E, 2, true); // UIQRModeSelect::Setup

        // 2. Restore track selection screen for Free Run (Mode 9)
        injector::MakeNOP(0x4B2AE3, 2, true);

        // 3. Restore track selection for Outrun (prevents forced randomization)
        injector::MakeNOP(0x4B2AA0, 2, true);
        injector::MakeNOP(0x5268F8, 2, true); // RaceStarter::SetupOutrun
    }

    void InstallSprintDriftOpponents()
    {
        // Restores AI opponents in Sprint Drift races
        injector::MakeNOP(0x4CD8D8, 2, true);                // UIQRModeOptions::SetupDrift
        injector::WriteMemory<uint8_t>(0x53F8B0, 0xEB, true); // RaceStarter::SetupDriftRace (fix 0 overwrite)
        injector::WriteMemory<uint8_t>(0x56DA0C, 0xA0, true); // DriftManager::BuildLeaderBoard (enable AI)
        injector::WriteMemory<uint8_t>(0x56DA25, 0x05, true); // NFSU1-style AI drift score generation
    }

    void InstallURLLapController()
    {
        // Restores the lap count modifier in URL race options
        injector::MakeNOP(0x4B3D4F, 2, true); // TONumLaps::Act
        injector::MakeNOP(0x4CD976, 2, true); // UIQRModeOptions::SetupURL
    }

    void InstallRestartRaceAllModes()
    {
        // Bypass the restrictions in PauseMenu::SetupRace that hide Restart Race in URL and Outrun
        // 0x004C2CB6: 74 60 (je 0x4c2d18)
        injector::WriteMemory<uint8_t>(0x4C2CB6, 0x90, true);
        injector::WriteMemory<uint8_t>(0x4C2CB7, 0x90, true);

        // 0x004C2CBB: 74 5B (je 0x4c2d18)
        injector::WriteMemory<uint8_t>(0x4C2CBB, 0x90, true);
        injector::WriteMemory<uint8_t>(0x4C2CBC, 0x90, true);

        // 0x004C2CC3: 74 53 (je 0x4c2d18)
        injector::WriteMemory<uint8_t>(0x4C2CC3, 0x90, true);
        injector::WriteMemory<uint8_t>(0x4C2CC4, 0x90, true);
    }
}
