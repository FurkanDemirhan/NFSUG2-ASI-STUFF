#include <windows.h>
#include <cstdio>
#include "Config.h"
#include "Logger.h"
#include "restorations/DebugCarCustomize.h"
#include "restorations/CameraRestorations.h"
#include "restorations/RaceModeRestorations.h"
#include "restorations/CustomizationRestorations.h"
#include "restorations/GameplayRestorations.h"

void InitRestorations()
{
    Logger::Init("NFSU2CodeRestoration.log");
    Logger::Log("[+] Target: SPEED2.EXE v1.2 US / NTSC (0x75BCC7)");

    // Load configurations from scripts/NFSU2CodeRestoration.ini
    g_Config.Load("scripts/NFSU2CodeRestoration.ini");
    Logger::Log("[+] Configuration loaded from scripts/NFSU2CodeRestoration.ini");

    if (g_Config.restoreDebugCarCustomize)
    {
        DebugCarCustomize::Install();
        Logger::Log("[+] Restored: Debug Car Customization screen (UI_DebugCarCustomize.fng)");
    }

    if (g_Config.restoreHiddenCameras)
    {
        CameraRestorations::Install();
        Logger::Log("[+] Restored: Hidden camera POV modes (Driver, Bumper, Hood, Drift, Chase)");
    }

    if (g_Config.restoreUnusedRaceModes)
    {
        RaceModeRestorations::Install();
        Logger::Log("[+] Restored: Outrun in QR, Free Run track select, Outrun track select");
    }

    if (g_Config.restoreSpecialVinyls)
    {
        CustomizationRestorations::Install();
        Logger::Log("[+] Restored: Special Vinyls hidden category (0x1C)");
    }

    GameplayRestorations::Install();
    Logger::Log("[+] Restored: Main loop tick and debugging hotkeys");

    Logger::Log("[+] All requested restorations applied successfully.");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)(base);
        IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);

        // Verify compatibility with NFS Underground 2 v1.2 NTSC (SPEED2.EXE, 4,800,512 bytes)
        uintptr_t entryPoint = base + nt->OptionalHeader.AddressOfEntryPoint + (0x400000 - base);
        if (entryPoint == 0x75BCC7)
        {
            InitRestorations();
        }
        else
        {
            // Incompatible executable version
            return TRUE;
        }
    }
    return TRUE;
}
