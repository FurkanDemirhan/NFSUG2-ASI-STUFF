#include "GameplayRestorations.h"
#include "BurnoutRestorations.h"
#include "VehicleDamageRestorations.h"
#include "DemoCareerRestorations.h"
#include "../GameAddresses.h"
#include "../Config.h"
#include "../Logger.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"

static bool s_AutoDriveActive = false;
static bool s_UnlockAllActive = false;

// Wine-compatible debounced key state: GetAsyncKeyState bit 0 ("pressed since last
// query") is unreliable under Wine - the high bit (0x8000, physically down) plus our own
// edge detection is what the VehicleHealth plugin uses successfully.
static bool s_AutoDriveWasDown = false;
static bool s_UnlockAllWasDown = false;
static bool s_DamageCycleWasDown = false;

static void GameTick()
{
    if (!g_Config.enableHotkeys)
        return;

    uint32_t flowManager = *(uint32_t*)GameAddresses::TheGameFlowManager;

    // 1. In-game hotkeys (GameFlowManager == 6)
    bool autoDriveDown = (GetAsyncKeyState(g_Config.hotkeyAutoDrive) & 0x8000) != 0;
    if (autoDriveDown && !s_AutoDriveWasDown)
    {
        if (flowManager == 6)
        {
            s_AutoDriveActive = !s_AutoDriveActive;
            uintptr_t playerPtr = *(uintptr_t*)GameAddresses::PlayersByNumber;
            if (playerPtr)
            {
                if (s_AutoDriveActive)
                {
                    GameAddresses::Player_AutoPilotOn((DWORD*)playerPtr);
                }
                else
                {
                    GameAddresses::Player_AutoPilotOff((DWORD*)playerPtr);
                    GameAddresses::Player_SetInputMode((void*)playerPtr, 2);
                }
                Logger::Log("[HOTKEY] F6 Autopilot %s", s_AutoDriveActive ? "ON" : "OFF");
            }
        }
    }
    s_AutoDriveWasDown = autoDriveDown;

    // 2. Unlock all things toggle (F5)
    bool unlockAllDown = (GetAsyncKeyState(g_Config.hotkeyUnlockAll) & 0x8000) != 0;
    if (unlockAllDown && !s_UnlockAllWasDown)
    {
        s_UnlockAllActive = !s_UnlockAllActive;
        *(uint8_t*)GameAddresses::UnlockAllThings = s_UnlockAllActive ? 1 : 0;
        if (s_UnlockAllActive)
        {
            injector::MakeNOP(0x50F0D8, 2, true); // Unlock individual performance parts
        }
        Logger::Log("[HOTKEY] F5 Unlock All %s", s_UnlockAllActive ? "ON" : "OFF");
    }
    s_UnlockAllWasDown = unlockAllDown;

    // 3. Cycle vehicle damage stage toggle (F7)
    bool damageCycleDown = (GetAsyncKeyState(g_Config.hotkeyDamageCycle) & 0x8000) != 0;
    if (damageCycleDown && !s_DamageCycleWasDown && g_Config.restoreVehicleDamage)
    {
        VehicleDamageRestorations::CyclePlayerDamage();
    }
    s_DamageCycleWasDown = damageCycleDown;
}

// Per-frame tick. Runs from the MainLoop ENTRY hook (see Install below) so it executes in
// EVERY game state and every frame. The previous hook point (0x581470, a __return
// placeholder in MainLoop's tail) sits behind eleven early returns/state branches and was
// never reached in the normal gameplay state - which killed the hotkeys and froze the
// damage wobble clock (parts stuck at one static rotation).
extern "C" __cdecl void MainLoopTick()
{
    static bool s_TickLogged = false;
    if (!s_TickLogged)
    {
        s_TickLogged = true;
        Logger::Log("[+] Main loop tick alive - hotkeys F5/F6/F7 active");
    }

    GameTick();
    BurnoutRestorations::Update(1.0f / 60.0f);
    if (g_Config.restoreVehicleDamage)
    {
        VehicleDamageRestorations::Update(1.0f / 60.0f);
    }
    if (g_Config.restoreEncounterOpponentCleanup)
    {
        DemoCareerRestorations::Update();
    }
}

// Naked entry cave at 0x5811C0 (MainLoop): runs the tick, then executes the replaced
// prologue ('sub esp,0xC; push ebx; push edi' - exactly 5 bytes) and resumes.
__attribute__((naked)) static void MainLoopEntryCave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "pushad\n"
        "call _MainLoopTick\n"
        "popad\n"
        "sub esp, 0xC\n"
        "push ebx\n"
        "push edi\n"
        "push 0x005811C5\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}

namespace GameplayRestorations
{
    void Install()
    {
        // Hook the MainLoop ENTRY (0x5811C0) - per-frame in every game state.
        injector::MakeJMP(GameAddresses::MainLoopEntry, (void*)MainLoopEntryCave, true);
    }
}
