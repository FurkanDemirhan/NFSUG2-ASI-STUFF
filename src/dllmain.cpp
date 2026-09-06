#include <windows.h>
#include <cstdio>
#include "includes/injector/injector.hpp"
#include "includes/injector/hooking.hpp"
#include "includes/IniReader.h"

// Forward declarations of restoration initializers
void InitRestorations();

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
        IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)(base);
        IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);

        // Check if executable is NFS Underground 2 v1.2 NTSC (SPEED2.EXE, 4.800.512 bytes)
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

void InitRestorations()
{
    // Minimal test initialization
    // Restorations will be wired here
}
