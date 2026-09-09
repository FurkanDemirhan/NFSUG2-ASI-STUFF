#include <windows.h>
#include "Logger.h"
#include "Config.h"
#include "VehicleHealthManager.h"
#include "HealthBarRenderer.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"


static void __cdecl VehicleHealth_MainLoopHook()
{
    static int s_Ticks = 0;
    if (s_Ticks++ < 10)
    {
        VehicleHealthLogger::Log("[+] VehicleHealth_MainLoopHook TICK #%d (Flow=%d)", s_Ticks, *(uint32_t*)0x008654A4);
    }

    HealthBarRenderer::CheckInstallHook();
    VehicleHealthManager::Update(1.0f / 60.0f);
}

void InitVehicleHealthMod()
{
    VehicleHealthLogger::Log("[+] Loading configuration from scripts/NFSU2VehicleHealth.ini...");
    g_HealthConfig.Load("scripts/NFSU2VehicleHealth.ini");

    VehicleHealthLogger::Log("[+] Config loaded: EnableHealthSystem=%d, DisplayHealthBars=%d, MaxHealth=%.1f, DamageMultiplier=%.2f",
                            g_HealthConfig.enableHealthSystem,
                            g_HealthConfig.displayHealthBars,
                            g_HealthConfig.maxHealth,
                            g_HealthConfig.damageMultiplier);

    if (!g_HealthConfig.enableHealthSystem)
    {
        VehicleHealthLogger::Log("[!] Health system is disabled in config. Mod will remain dormant.");
        return;
    }

    // Initialize vehicle health tracking, collision hooks, and code caves
    VehicleHealthLogger::Log("[+] Initializing VehicleHealthManager hooks...");
    VehicleHealthManager::Init();
    VehicleHealthLogger::Log("[+] VehicleHealthManager hooks initialized.");

    // Initialize Direct3D 9 health bar renderer
    VehicleHealthLogger::Log("[+] Initializing HealthBarRenderer...");
    HealthBarRenderer::Init();
    VehicleHealthLogger::Log("[+] HealthBarRenderer initialized.");

    // Hook main game loop tick at 0x00581475 (dedicated call 0x4022c0, unshared with other mods)
    VehicleHealthLogger::Log("[+] Hooking main game loop at 0x00581475 (dedicated slot)...");
    injector::MakeCALL(0x00581475, (void*)VehicleHealth_MainLoopHook, true);
    VehicleHealthLogger::Log("[+] Main game loop hook installed at 0x00581475.");

    VehicleHealthLogger::Log("[+] NFSU2 Vehicle Health & Damage Mod initialized successfully!");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        VehicleHealthLogger::Init("scripts/NFSU2VehicleHealth.log");
        VehicleHealthLogger::Log("[+] DllMain: DLL_PROCESS_ATTACH received for NFSU2VehicleHealth.asi");

        uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)(base);
        IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);

        // Verify compatibility with NFS Underground 2 v1.2 NTSC (SPEED2.EXE, 4,800,512 bytes)
        uintptr_t entryPoint = base + nt->OptionalHeader.AddressOfEntryPoint + (0x400000 - base);
        VehicleHealthLogger::Log("[+] Target Executable Base: 0x%08lX, EntryPoint: 0x%08lX", (unsigned long)base, (unsigned long)entryPoint);

        if (entryPoint == 0x75BCC7)
        {
            VehicleHealthLogger::Log("[+] Compatible SPEED2.EXE v1.2 NTSC detected (0x75BCC7). Proceeding with initialization.");
            InitVehicleHealthMod();
        }
        else
        {
            VehicleHealthLogger::Log("[!] Incompatible executable version (EntryPoint: 0x%08lX != 0x75BCC7). Mod aborted.", (unsigned long)entryPoint);
            return TRUE;
        }
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        VehicleHealthLogger::Log("[+] DllMain: DLL_PROCESS_DETACH received. Closing log.");
        VehicleHealthLogger::Close();
    }
    return TRUE;
}
