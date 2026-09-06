#include "DebugCarCustomize.h"
#include "../GameAddresses.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"

// Naked code cave to inject Debug option into ChooseCustomizeCategory menu
__attribute__((naked)) static void DebugCarCustomizeCodeCave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "mov edx, [edi]\n"
        "push eax\n"
        "mov ecx, edi\n"
        "call dword ptr [edx + 0x18]\n"
        "push 0x4C\n"
        "call 0x575620\n"        // j__malloc
        "add esp, 4\n"
        "cmp eax, ebx\n"
        "je 1f\n"
        "push ebx\n"
        "push 0xE79A53F8\n"      // "Debug" string hash
        "push 0x74CE8C0B\n"      // UI_ICON_DEBUG
        "push 0x04\n"            // StateID 4 = UI_DebugCarCustomize.fng
        "mov ecx, eax\n"
        "call 0x520CB0\n"        // AddElementToMenuWithStateID
        "push eax\n"
        "mov ecx, edi\n"
        "call dword ptr [edx + 0x18]\n"
        "1:\n"
        "mov eax, [edi + 4]\n"
        "push 0x520F57\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}

namespace DebugCarCustomize
{
    void Install()
    {
        // Hook ChooseCustomizeCategory::Setup to add Debug button
        injector::MakeJMP(0x520F4C, (void*)DebugCarCustomizeCodeCave, true);
    }
}
