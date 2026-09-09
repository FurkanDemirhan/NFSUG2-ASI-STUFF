#include "EngineFixes.h"
#include "../GameAddresses.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"

namespace EngineFixes
{
    void InstallWheelFix()
    {
        // Fixes the disappearing wheels bug in CarPartCuller::CullParts
        injector::WriteMemory<uint8_t>(0x60C5A9, 0x01, true);
    }

    void InstallDebugWorldCamera()
    {
        // Activates the game's internal DebugWorldCameraMover system
        constexpr uintptr_t DebugCamerasEnabled = 0x865098;
        injector::WriteMemory<uint8_t>(DebugCamerasEnabled, 0x01, true);
    }

    // Helper to recover the active audio/sound path bank slot if the global ds:0x8B7BB0 was cleared
    extern "C" uintptr_t __cdecl GetOrRecoverCurrentPathBank()
    {
        uintptr_t bank = *(uintptr_t*)0x008B7BB0;
        if (bank != 0)
        {
            uintptr_t p38 = *(uintptr_t*)(bank + 0x38);
            uintptr_t p34 = *(uintptr_t*)(bank + 0x34);
            if (p38 != 0 && p34 != 0)
            {
                return bank;
            }
        }

        // Search the 4 static bank slots in SPEED2.EXE (slot size 0x928):
        // Slot 0: 0x008B5710, Slot 1: 0x008B6038, Slot 2: 0x008B6960, Slot 3: 0x008B7288
        for (int i = 0; i < 4; i++)
        {
            uintptr_t slot = 0x008B5710 + (uintptr_t)(i * 0x928);
            uintptr_t p38 = *(uintptr_t*)(slot + 0x38);
            uintptr_t p34 = *(uintptr_t*)(slot + 0x34);
            if (p38 != 0 && p34 != 0)
            {
                *(uintptr_t*)0x008B7BB0 = slot;
                return slot;
            }
        }

        return 0;
    }

    // Code cave at 0x00739274 (PATH_status) preventing fatal 0xC0000005 crash on null ds:0x8B7BB0
    __attribute__((naked)) static void PathStatusCrashFixCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            // 1. Original instruction: mov [esp+0x10], edx
            "mov dword ptr [esp + 0x10], edx\n"

            // 2. Preserve registers around C function call
            "push ecx\n"
            "push edx\n"
            "call _GetOrRecoverCurrentPathBank\n"
            "pop edx\n"
            "pop ecx\n"

            // 3. Test if valid bank pointer returned in eax
            "test eax, eax\n"
            "jz 1f\n"

            // 4. Success path: dereference [eax + 0x38] and execute instruction at 0x739280
            "mov edx, dword ptr [eax + 0x38]\n"
            "mov dword ptr [esi], ecx\n"
            "push 0x00739282\n"
            "ret\n"

            // 5. Fallback path when no bank is loaded / null:
            // Safely zero out values and bypass to 0x007392CF
            "1:\n"
            "xor ebp, ebp\n"
            "mov dword ptr [esp + 0x14], 0\n"
            "push 0x007392CF\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    void InstallPathStatusCrashFix()
    {
        // Hook 0x00739274 (size 12 bytes through 0x00739280) in PATH_status
        injector::MakeJMP(0x00739274, (void*)PathStatusCrashFixCodeCave, true);
        injector::MakeNOP(0x00739274 + 5, 7, true);
    }
}
