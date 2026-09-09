#include "RaceModeRestorations.h"
#include "FEngRestorations.h"
#include "../GameAddresses.h"
#include "../Config.h"
#include "../Logger.h"
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

extern "C" const char* __cdecl Burnout_GetHUDPackageName() asm("_Burnout_GetHUDPackageName");
const char* __cdecl Burnout_GetHUDPackageName()
{
    FEngRestorations::InstallEmbeddedHUDCarShow();
    return "HUD_CarShow.fng";
}

__attribute__((naked)) static void Burnout_HUDChooser_Cave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "mov al, byte ptr [0x89E7E2]\n" // isBurnout
        "test al, al\n"
        "jz 1f\n"
        // In Burnout mode, install embedded HUD_CarShow.fng and assign
        "push ecx\n"
        "push edx\n"
        "call _Burnout_GetHUDPackageName\n"
        "pop edx\n"
        "pop ecx\n"
        "mov dword ptr [esp + 0x14], eax\n"
        "push 0x005F19EA\n"
        "ret\n"
        "1:\n"
        // Normal circuit HUD (HUD_SingleRace.fng)
        "push 0x005F19B2\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}

extern "C" void __cdecl SetupBurnout_Custom()
{
    FEngRestorations::InstallEmbeddedHUDCarShow();
    *(uint8_t*)0x0089E7E2 = 1; // isBurnout = 1
    *(uint8_t*)0x0089E7D9 = 0; // isDrift = 0

    uint8_t numPlayers = *(uint8_t*)0x0083ABA0;
    if (numPlayers == 0)
    {
        numPlayers = 1;
    }
    uint8_t numOpponents = *(uint8_t*)0x0083ABA1;
    if (numOpponents > 3)
    {
        numOpponents = 3;
    }
    uint8_t difficulty = *(uint8_t*)0x0083ABA4;
    uint8_t catchup = *(uint8_t*)0x0083ABA5;
    uint16_t trackId = *(uint16_t*)0x0083ABA8;
    uint16_t laps = *(uint16_t*)0x0083ABAA;
    uint32_t direction = *(uint32_t*)0x0083ABAC;

    if (trackId == 0 || trackId == 4097)
    {
        trackId = 4001; // Default to City Circuit 1
    }
    if (laps == 0)
    {
        void* trackInfo = ((void* (__cdecl*)(int))0x005D3E40)(trackId);
        if (trackInfo)
        {
            laps = *(uint8_t*)((uintptr_t)trackInfo + 0x85);
        }
        if (laps == 0)
        {
            laps = 2;
        }
    }

    *(uint32_t*)0x0089E7C0 = numPlayers;
    *(uint32_t*)0x0089E7C4 = numOpponents;
    *(uint32_t*)0x0089E7B0 = 1;
    *(uint32_t*)0x0089E7A8 = difficulty;
    *(uint32_t*)0x0089E7F8 = 0;
    *(float*)0x0089E7FC = 1.0f;
    *(float*)0x0089E800 = 1.0f;
    *(uint32_t*)0x0089E7F4 = catchup;
    *(uint8_t*)0x0089E7D2 = 0; // Disable smokeshow solo timeup timeout so standard laps apply
    *(uint32_t*)0x0089E7A0 = trackId;
    *(uint32_t*)0x0089E7A4 = direction;
    *(uint32_t*)0x0089E7B4 = laps;
    *(uint32_t*)0x0089E7BC = laps;
    *(uint32_t*)0x007F68B0 = *(uint8_t*)0x0083AA1B;

    Logger::Log("[Burnout] SetupBurnout_Custom: track=%d, laps=%d, dir=%d, players=%d, opps=%d, diff=%d",
        trackId, laps, direction, numPlayers, numOpponents, difficulty);

    // Call RaceStarter::SetupPlayerRacers(numPlayers, 0)
    ((void (__cdecl*)(int, int))0x00525ED0)(numPlayers, 0);

    if (numOpponents > 0)
    {
        ((void (__cdecl*)(int))0x0053EE90)(numOpponents); // RaceStarter::SetupOpponents
        ((void (__cdecl*)(int))0x005264A0)(3);            // SetupGrid(3) -> 4-car starting grid
        ((void (__cdecl*)())0x004FE9F0)();               // SetupStartingGrid()
    }
    else
    {
        ((void (__cdecl*)(int))0x005264A0)(1);            // SetupGrid(1) -> 1-car grid
        ((void (__cdecl*)())0x004FE9F0)();               // SetupStartingGrid()
    }
}

extern "C" void __cdecl UIQRModeOptions_Setup_Dispatcher(void* screen)
{
    uint32_t mode = *(uint32_t*)0x0083AAB4;
    Logger::Log("[QR] UIQRModeOptions_Setup_Dispatcher mode=%d, screen=0x%p", mode, screen);
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
    case 11: // Burnout (use Circuit options: Track, Laps, Direction, Opponents, Difficulty)
    {
        uint8_t* pNumPlayers = (uint8_t*)0x0083ABA0;
        if (*pNumPlayers == 0)
        {
            *pNumPlayers = 1;
        }
        uint8_t* pNumOpponents = (uint8_t*)0x0083ABA1;
        if (*pNumOpponents > 3)
        {
            *pNumOpponents = 3;
        }
        uint16_t* pTrackId = (uint16_t*)0x0083ABA8;
        if (*pTrackId == 0 || *pTrackId == 4097)
        {
            *pTrackId = 4001;
        }
        uint16_t* pLaps = (uint16_t*)0x0083ABAA;
        if (*pLaps == 0)
        {
            *pLaps = 2;
        }
        ((SetupFn)0x004CCF60)(screen);
        break;
    }
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
        uint8_t* pNumPlayers = (uint8_t*)0x0083ABA0;
        if (*pNumPlayers == 0)
        {
            *pNumPlayers = 1;
        }
        uint8_t* pNumOpponents = (uint8_t*)0x0083ABA1;
        if (*pNumOpponents > 3)
        {
            *pNumOpponents = 3;
        }
        uint16_t* pTrackId = (uint16_t*)0x0083ABA8;
        if (*pTrackId == 0 || *pTrackId == 4097)
        {
            *pTrackId = 4001;
        }
        uint16_t* pLaps = (uint16_t*)0x0083ABAA;
        if (*pLaps == 0)
        {
            *pLaps = 2;
        }
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
            // 0xBF5AC5A9 = Official Lap Knockout icon from mode icon table
            // 0x7FD73849 = Official "Lap KO" text label from English.bin (localized across all languages)
            constructFn(itemKO, 0xBF5AC5A9, 0x7FD73849, 0);
            *(uint32_t*)itemKO = (uint32_t)&s_Vtable_MSLapKO;
            addItemFn(screen, itemKO);
        }
    }

    if (g_Config.restoreBurnoutMode)
    {
        void* itemBurnout = allocFn(0x48);
        if (itemBurnout)
        {
            // 0x00119144 = Drift tire-smoke icon from mode icon table
            // 0x63078970 = Official "Burnout" text label from English.bin (localized across all languages)
            constructFn(itemBurnout, 0x00119144, 0x63078970, 0);
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

// Minimap safe call caves to prevent NULL dereference crashes when HUD lacks minimap element (HUD_Drift.fng)
__attribute__((naked)) static void Minimap_Tick_SafeCall_Cave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "test eax, eax\n"
        "jz 1f\n"
        "mov ecx, eax\n"
        "mov edx, 0x004AC2B0\n"
        "call edx\n"
        "push 0x004CA912\n"
        "ret\n"
        "1:\n"
        "add esp, 8\n"       // Clean the 2 pushed arguments for 0x4AC2B0
        "pop edi\n"          // Restore saved registers from Tick__7Minimap prologue
        "pop esi\n"
        "pop ebx\n"
        "add esp, 0x64\n"    // Clean local stack frame
        "ret\n"              // Exit Tick__7Minimap cleanly
        ".att_syntax prefix\n"
    );
}

__attribute__((naked)) static void Minimap_Coords_SafeCall_Cave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "test eax, eax\n"
        "jz 1f\n"
        "mov ecx, eax\n"
        "mov edx, 0x004AC2B0\n"
        "call edx\n"
        "push 0x004CAEDD\n"
        "ret\n"
        "1:\n"
        "add esp, 8\n"       // Clean the 2 pushed arguments for 0x4AC2B0
        "xor al, al\n"       // Return false (cannot convert coords without minimap)
        "mov esp, ebp\n"
        "pop ebp\n"
        "ret 0x10\n"         // Exit sub_004cae60 cleanly
        ".att_syntax prefix\n"
    );
}



// Hook post-race mode check at 0x4D7240 to route Drift tracks to drift score results and others to circuit results
__attribute__((naked)) static void PostRace_ModeCheck_Cave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "push ebp\n"
        "push esi\n"
        "mov ebp, ecx\n"
        "mov al, byte ptr [0x89E7D9]\n" // isDrift
        "test al, al\n"
        "jne 1f\n"
        "mov eax, dword ptr [0x89E7A0]\n"
        "cmp eax, 0x104E\n"
        "jl 2f\n"
        "cmp eax, 0x1053\n"
        "jg 2f\n"
        "1:\n"
        "push 0x004D7257\n"             // Drift post-race results (0x4bf140)
        "ret\n"
        "2:\n"
        "push 0x004D7279\n"             // Circuit post-race results (0x4bf290)
        "ret\n"
        ".att_syntax prefix\n"
    );
}

// Hook circuit post-race entry at 0x4BF2CF to prevent NULL dereference crashes when finisher is missing
__attribute__((naked)) static void PostRace_Circuit_SafeEntry_Cave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "lea edx, [edi + 0x4c]\n"       // edx = sentinel head of finisher list
        "cmp eax, edx\n"                // empty list check (bList::Get returns head when empty)
        "je 1f\n"
        "cmp eax, ecx\n"
        "je 1f\n"
        "mov ebp, dword ptr [eax + 8]\n"
        "test ebp, ebp\n"
        "jz 1f\n"
        "mov eax, dword ptr [ebp + 4]\n"
        "push 0x004BF2D5\n"
        "ret\n"
        "1:\n"
        "push 0x004BF4BD\n"             // Clean exit of sub_004bf290 (pop ebp; pop ebx; pop edi; ...)
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

        // Install post-race crash protection
        injector::MakeRangedNOP(0x004BF2CF, 0x004BF2D5, true);
        injector::MakeJMP(0x004BF2CF, (void*)PostRace_Circuit_SafeEntry_Cave, true);

        injector::MakeRangedNOP(0x004D7240, 0x004D7257, true);
        injector::MakeJMP(0x004D7240, (void*)PostRace_ModeCheck_Cave, true);
    }

    void InstallBurnoutMode()
    {
        // 0. Ensure default parameters for Mode 11 are initialized in RaceParameters table
        uint8_t* pNumPlayers = (uint8_t*)0x0083ABA0;
        if (*pNumPlayers == 0)
        {
            *pNumPlayers = 1;
        }
        uint8_t* pNumOpponents = (uint8_t*)0x0083ABA1;
        if (*pNumOpponents > 3)
        {
            *pNumOpponents = 3;
        }
        uint16_t* pTrackId = (uint16_t*)0x0083ABA8;
        if (*pTrackId == 0 || *pTrackId == 4097)
        {
            *pTrackId = 4001;
        }
        uint16_t* pLaps = (uint16_t*)0x0083ABAA;
        if (*pLaps == 0)
        {
            *pLaps = 2;
        }

        // 1. Hook entire SetupBurnout function at 0x526720 with custom opponents and laps setup
        injector::MakeJMP(0x00526720, (void*)SetupBurnout_Custom, true);

        // 2. Ensure UIQRModeOptions::Setup is hooked for Burnout options
        static bool s_ModeOptionsHooked = false;
        if (!s_ModeOptionsHooked)
        {
            injector::MakeRangedNOP(0x4E2C8C, 0x4E2CA1, true);
            injector::MakeJMP(0x4E2C8C, (void*)UIQRModeOptions_Setup_Cave, true);
            s_ModeOptionsHooked = true;
        }

        // 3. Hook Minimap tick and coords conversion to prevent NULL dereference crashes when HUD lacks minimap
        injector::MakeRangedNOP(0x004CA90B, 0x004CA912, true);
        injector::MakeJMP(0x004CA90B, (void*)Minimap_Tick_SafeCall_Cave, true);

        injector::MakeRangedNOP(0x004CAED6, 0x004CAEDD, true);
        injector::MakeJMP(0x004CAED6, (void*)Minimap_Coords_SafeCall_Cave, true);

        // 4. Route Burnout mode HUD to HUD_CarShow.fng (falling back to HUD_SingleRace.fng)
        injector::MakeRangedNOP(0x005F19A2, 0x005F19B2, true);
        injector::MakeJMP(0x005F19A2, (void*)Burnout_HUDChooser_Cave, true);

        // 5. Hook post-race mode check at 0x4D7240 to route Burnout to Circuit results
        injector::MakeRangedNOP(0x004D7240, 0x004D7257, true);
        injector::MakeJMP(0x004D7240, (void*)PostRace_ModeCheck_Cave, true);

        // 6. Hook circuit post-race entry at 0x4BF2CF to prevent NULL dereference crashes
        injector::MakeRangedNOP(0x004BF2CF, 0x004BF2D5, true);
        injector::MakeJMP(0x004BF2CF, (void*)PostRace_Circuit_SafeEntry_Cave, true);
    }
}
