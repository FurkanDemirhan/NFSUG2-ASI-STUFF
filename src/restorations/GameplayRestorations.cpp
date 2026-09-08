#include "GameplayRestorations.h"
#include "BurnoutRestorations.h"
#include "VehicleDamageRestorations.h"
#include "../GameAddresses.h"
#include "../Config.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"

static bool s_AutoDriveActive = false;
static bool s_UnlockAllActive = false;

static void GameTick()
{
    if (!g_Config.enableHotkeys)
        return;

    bool isLostFocus = *(bool*)GameAddresses::IsLostFocus;
    if (isLostFocus)
        return;

    uint32_t flowManager = *(uint32_t*)GameAddresses::TheGameFlowManager;

    // 1. In-game hotkeys (GameFlowManager == 6)
    if (flowManager == 6)
    {
        // Autopilot / AutoDrive toggle (F6)
        if (GetAsyncKeyState(g_Config.hotkeyAutoDrive) & 1)
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
            }
        }
    }

    // 2. Unlock all things toggle (F5)
    if (GetAsyncKeyState(g_Config.hotkeyUnlockAll) & 1)
    {
        s_UnlockAllActive = !s_UnlockAllActive;
        *(uint8_t*)GameAddresses::UnlockAllThings = s_UnlockAllActive ? 1 : 0;
        if (s_UnlockAllActive)
        {
            injector::MakeNOP(0x50F0D8, 2, true); // Unlock individual performance parts
        }
    }

    // 3. Cycle vehicle damage stage toggle (F7)
    if (g_Config.restoreVehicleDamage && (GetAsyncKeyState(g_Config.hotkeyDamageCycle) & 1))
    {
        VehicleDamageRestorations::CyclePlayerDamage();
    }
}

// Periodic callback called from the main game loop at 0x581470
static void __cdecl MainLoopHook()
{
    GameTick();
    BurnoutRestorations::Update(1.0f / 60.0f);
    if (g_Config.restoreVehicleDamage)
    {
        VehicleDamageRestorations::Update(1.0f / 60.0f);
    }
}

namespace GameplayRestorations
{
    void Install()
    {
        // Hook the main game loop at 0x581470
        injector::MakeCALL(GameAddresses::MainLoopHook, (void*)MainLoopHook, true);
    }
}
