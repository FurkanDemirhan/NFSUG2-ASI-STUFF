#include "RaceModeRestorations.h"
#include "../GameAddresses.h"
#include "../Config.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"

// C-linkage helper invoked from BarrierCodeCave
extern "C" void __cdecl HandleBarrierSetup(int trackId)
{
    // Bayview Free Roam (Track 4000) and dummy stubs lack barrier packages (BARRIERS_xxxx).
    // Requesting missing barrier packages causes TrackStreamer to crash during race loading.
    bool lacksBarriers = (trackId == 4000 || trackId == 1001 || trackId == 1002 || 
                          trackId == 1003 || trackId == 1099 || trackId == 1102 || 
                          trackId == 3001 || trackId == 4200 || trackId == 4300 || 
                          trackId == 4400 || trackId == 4600 || trackId == 4700 || 
                          trackId == 4800 || trackId == 4900);

    bool isDriftOrURL = (trackId >= 4300 && trackId <= 4399) || (trackId >= 4600);
    bool shouldRemove = false;

    if (lacksBarriers)
    {
        // Must remove to bypass TrackStreamer registration and prevent crash
        shouldRemove = true;
    }
    else if (g_Config.removeRaceBarriers)
    {
        // If user requested removing race barriers, keep them only for drift and URL
        if (!isDriftOrURL)
        {
            shouldRemove = true;
        }
    }

    if (shouldRemove)
    {
        *(uint8_t*)0x7A078B = 'M'; // "PLAYER_BARRMERS_%d"
        *(uint8_t*)0x7A0798 = 'M'; // "BARRMERS_%d"
    }
    else
    {
        *(uint8_t*)0x7A078B = 'I'; // "PLAYER_BARRIERS_%d"
        *(uint8_t*)0x7A0798 = 'I'; // "BARRIERS_%d"
    }
}

// Naked code cave hooked at 0x578070 inside sub_578060
__attribute__((naked)) static void BarrierCodeCave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "movsx eax, word ptr [eax + 0x8A]\n" // Original instruction: read trackId from RaceParameters
        "pushad\n"                           // Preserve general registers
        "push eax\n"                         // Pass trackId argument
        "call _HandleBarrierSetup\n"         // 32-bit cdecl symbol with leading underscore
        "add esp, 4\n"                       // Clean argument
        "popad\n"                            // Restore registers (eax still has trackId)
        "push 0x578077\n"                    // Return to SPEED2.EXE (mov dword ptr [esp], eax)
        "ret\n"
        ".att_syntax prefix\n"
    );
}

// Track availability filter for Quick Race menus
static bool __stdcall UIQRTrackSelect_IsAvailable(int TrackInfoBlock, int eTrackDirection)
{
    if (!TrackInfoBlock)
        return false;

    uint16_t trackId = *(uint16_t*)(TrackInfoBlock + 138);

    // Filter out known crashy / unpopulated dummy track IDs
    if (trackId == 1001 || trackId == 1002 || trackId == 1003 || trackId == 1099 || 
        trackId == 1102 || trackId == 3001 || trackId == 4200 || trackId == 4300 || 
        trackId == 4400 || trackId == 4600 || trackId == 4700 || trackId == 4800 || 
        trackId == 4900)
    {
        return false;
    }

    return true;
}

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
        // Unfreeze lap and opponent controllers in race mode options
        injector::WriteMemory<uint8_t>(0x4B3E14, 0xEB, true); // Draw__9TONumLaps: Don't freeze laps
        injector::WriteMemory<uint8_t>(0x4B3903, 0xEB, true); // Draw__18MO_QR_NumOpponents: Don't freeze opponents
        injector::WriteMemory<uint8_t>(0x4B4068, 0xEB, true); // Act__7TOLapKO: Don't copy static numbers
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

    void InstallBarrierCrashFix()
    {
        // Hook sub_578060 at 0x578070 (movsx eax, word ptr [eax + 0x8A])
        // Instruction size is 7 bytes (0x578070 to 0x578077).
        injector::MakeRangedNOP(0x578070, 0x578077, true);
        injector::MakeJMP(0x578070, (void*)BarrierCodeCave, true);
    }

    void InstallCareerLockedBarriersFix()
    {
        // 0x7A073C points to 'I' in "BARRIERS_CAREER%d". Writing 'M' turns it into "BARRMERS_CAREER%d",
        // preventing the engine from streaming neon barriers locking city districts in career mode.
        injector::WriteMemory<uint8_t>(0x7A073C, 'M', true);
    }

    void InstallAnyTrackInAnyMode()
    {
        // Hook UIQRTrackSelect::BuildPresetTrackList at 0x4CDEF5
        // Replaces vanilla UIQRTrackSelect_IsAvailable_Game (0x4987C0) to allow all valid tracks
        // including Track 4000 (Bayview City) across all quick race modes.
        injector::MakeCALL(0x4CDEF5, (void*)UIQRTrackSelect_IsAvailable, true);
    }
}
