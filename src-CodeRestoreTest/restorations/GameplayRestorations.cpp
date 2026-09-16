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

// Per-frame tick hook installed at 0x005814DB (dedicated unshared 'call __return' slot).
// 0x5811C0 is only the function prologue of MainLoop() (called once by main() at boot),
// whereas 0x5814DB sits inside the core while-loop and executes once per frame in every game state.
extern "C" __cdecl void MainLoopTick()
{
    static int s_Ticks = 0;
    s_Ticks++;
    if (s_Ticks <= 5 || s_Ticks % 1200 == 0)
    {
        Logger::Log("[+] Main loop tick #%d alive (Flow=%d) - hotkeys F5/F6/F7 active", 
                    s_Ticks, *(uint32_t*)GameAddresses::TheGameFlowManager);
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

namespace GameplayRestorations
{
    void Install()
    {
        // Hook 0x005814DB (dedicated 'call 0x4022c0' slot in MainLoop)
        injector::MakeCALL(GameAddresses::MainLoopTickHook, (void*)MainLoopTick, true);
        Logger::Log("[+] Main loop tick hook installed at 0x%08X", (unsigned int)GameAddresses::MainLoopTickHook);
    }
}
