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
                          trackId == 3001 || trackId == 4097 || trackId == 4200 || 
                          trackId == 4300 || trackId == 4400 || trackId == 4600 || 
                          trackId == 4700 || trackId == 4800 || trackId == 4900);

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
        trackId == 1102 || trackId == 3001 || trackId == 4097 || trackId == 4200 || 
        trackId == 4300 || trackId == 4400 || trackId == 4600 || trackId == 4700 || 
        trackId == 4800 || trackId == 4900)
    {
        return false;
    }

    return true;
}

// Helpers and caves for Burnout and Scrapped Race Modes

extern "C" void __cdecl SetupBurnout_TrackHelper()
{
    // Read player's selected track from RaceParameters for Mode 11 (0x0083ABA8)
    uint16_t trackId = *(uint16_t*)0x0083ABA8;
    if (trackId == 0 || trackId == 4097)
    {
        trackId = 4001; // Default to Track 1 if unselected or original crashy 4097
    }

    uint32_t direction = *(uint32_t*)0x0083ABAC;
    uint16_t laps = *(uint16_t*)0x0083ABAA;
    if (laps == 0)
    {
        laps = 2;
    }

    *(uint32_t*)0x0089E7A0 = trackId;
    *(uint32_t*)0x0089E7A4 = direction;
    *(uint32_t*)0x0089E7BC = laps;
}

__attribute__((naked)) static void SetupBurnout_TrackCave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "pushad\n"
        "call _SetupBurnout_TrackHelper\n"
        "popad\n"
        "push 0x00526808\n" // Return to SetupBurnout (mov dword ptr [0x89e7b4], esi)
        "ret\n"
        ".att_syntax prefix\n"
    );
}

extern "C" void __cdecl UIQRModeOptions_Setup_Dispatcher(void* screen)
{
    uint32_t mode = *(uint32_t*)0x0083AAB4;
    typedef void (__thiscall* SetupFn)(void* screen);

    switch (mode)
    {
    case 0: // Circuit
        ((SetupFn)0x004CCF60)(screen);
        break;
    case 1: // Sprint
        ((SetupFn)0x004CD2D0)(screen);
        break;
    case 2: // Drag
        ((SetupFn)0x004CD530)(screen);
        break;
    case 3: // Drift
        ((SetupFn)0x004CD7A0)(screen);
        break;
    case 4: // URL
    case 5:
        ((SetupFn)0x004B47B0)(screen);
        break;
    case 7: // Lap Knockout / GT
        ((SetupFn)0x004CDB40)(screen);
        break;
    case 8: // Outrun
        ((SetupFn)0x004CDA20)(screen);
        break;
    case 9: // Free Run
        ((SetupFn)0x004B4A10)(screen);
        break;
    case 10: // Street X
        ((SetupFn)0x004B47B0)(screen);
        break;
    case 11: // Burnout
        ((SetupFn)0x004CD7A0)(screen);
        break;
    default:
        break;
    }

    // Call BuildLayout(screen, 0)
    typedef void (__thiscall* BuildLayoutFn)(void* screen, int unk);
    ((BuildLayoutFn)0x00539650)(screen, 0);
}

__attribute__((naked)) static void UIQRModeOptions_Setup_Cave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "push esi\n"
        "call _UIQRModeOptions_Setup_Dispatcher\n"
        "add esp, 4\n"
        "pop esi\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}

struct MenuItemVtable
{
    void (__thiscall* dtor)(void* self, bool freeMemory);
    void (__thiscall* react)(void* self, const char* name, unsigned int event, void* feObj, unsigned int p1, unsigned int p2);
};

static void __thiscall MSBurnout_React(void* self, const char* name, unsigned int event, void* feObj, unsigned int p1, unsigned int p2)
{
    if (event == 0x0C407210) // Select/Enter
    {
        ((void (__cdecl*)(int))0x004B2C00)(11); // Mode 11: Burnout
    }
}

static void __thiscall MSLapKO_React(void* self, const char* name, unsigned int event, void* feObj, unsigned int p1, unsigned int p2)
{
    if (event == 0x0C407210) // Select/Enter
    {
        ((void (__cdecl*)(int))0x004B2C00)(7); // Mode 7: Lap Knockout / GT
    }
}

static const MenuItemVtable s_Vtable_MSBurnout = {
    (void (__thiscall*)(void*, bool))0x004AEB70,
    MSBurnout_React
};

static const MenuItemVtable s_Vtable_MSLapKO = {
    (void (__thiscall*)(void*, bool))0x004AEB70,
    MSLapKO_React
};

extern "C" void __cdecl AddExtraQRModes(void* screen)
{
    typedef void* (__cdecl* AllocFn)(size_t);
    typedef void* (__thiscall* ConstructItemFn)(void*, uint32_t, uint32_t, uint32_t);
    typedef void (__thiscall* AddItemFn)(void*, void*);

    AllocFn allocFn = (AllocFn)0x00575620;
    ConstructItemFn constructFn = (ConstructItemFn)0x0051F670;
    AddItemFn addItemFn = (AddItemFn)(*(uint32_t*)(*(uint32_t*)screen + 0x18));

    if (g_Config.restoreLapKOInQR)
    {
        void* itemKO = allocFn(0x48);
        if (itemKO)
        {
            // 0x03932CDC = "GT" label in English.bin; 0xe9638d3e = Circuit icon
            constructFn(itemKO, 0x03932CDC, 0xe9638d3e, 0);
            *(uint32_t*)itemKO = (uint32_t)&s_Vtable_MSLapKO;
            addItemFn(screen, itemKO);
        }
    }

    if (g_Config.restoreBurnoutMode)
    {
        void* itemBurnout = allocFn(0x48);
        if (itemBurnout)
        {
            // 0x63078970 = "Burnout" label in English.bin; 0x119144 = Drift icon
            constructFn(itemBurnout, 0x63078970, 0x119144, 0);
            *(uint32_t*)itemBurnout = (uint32_t)&s_Vtable_MSBurnout;
            addItemFn(screen, itemBurnout);
        }
    }
}

__attribute__((naked)) static void UIQRModeSelect_Setup_Cave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "pushad\n"
        "push esi\n"
        "call _AddExtraQRModes\n"
        "add esp, 4\n"
        "popad\n"
        "mov eax, [esi + 4]\n"
        "mov edi, [esi + 0x4C]\n"
        "push 0x004B2FE6\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
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

    void InstallScrappedRaceModes()
    {
        // Hook UIQRModeSelect::Setup at 0x4B2FE0 to add Burnout and Lap KO / GT to the mode menu
        injector::MakeRangedNOP(0x4B2FE0, 0x4B2FE6, true);
        injector::MakeJMP(0x4B2FE0, (void*)UIQRModeSelect_Setup_Cave, true);

        // Ensure UIQRModeOptions::Setup is hooked for all mode options
        static bool s_ModeOptionsHooked = false;
        if (!s_ModeOptionsHooked)
        {
            injector::MakeRangedNOP(0x4E2C8C, 0x4E2CA1, true);
            injector::MakeJMP(0x4E2C8C, (void*)UIQRModeOptions_Setup_Cave, true);
            s_ModeOptionsHooked = true;
        }
    }

    void InstallBurnoutMode()
    {
        // 1. Hook SetupBurnout at 0x5267F8 to read player's chosen track rather than forcing 4097
        injector::MakeRangedNOP(0x5267F8, 0x526808, true);
        injector::MakeJMP(0x5267F8, (void*)SetupBurnout_TrackCave, true);

        // 2. Ensure UIQRModeOptions::Setup is hooked for Burnout options
        static bool s_ModeOptionsHooked = false;
        if (!s_ModeOptionsHooked)
        {
            injector::MakeRangedNOP(0x4E2C8C, 0x4E2CA1, true);
            injector::MakeJMP(0x4E2C8C, (void*)UIQRModeOptions_Setup_Cave, true);
            s_ModeOptionsHooked = true;
        }
    }
}
