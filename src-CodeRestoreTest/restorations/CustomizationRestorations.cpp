#include "CustomizationRestorations.h"
#include "../GameAddresses.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"

// Naked code cave to inject Special Vinyl category (0x1C) into the Vinyl Category menu
__attribute__((naked)) static void VinylCategoryCodeCave()
{
    asm volatile (
        ".intel_syntax noprefix\n"
        "push 0xE79A53F8\n"      // Debug String Hash
        "push 0x93EC1CE9\n"      // NFSU2 Icon Hash
        "push 0x1C\n"            // Hidden vinyl category ID
        "mov ecx, esi\n"
        "mov eax, 0x545920\n"
        "call eax\n"        // AddCustomElementToMenu
        "mov eax, [esi + 4]\n"
        "push eax\n"
        "mov edx, 0x505450\n"
        "call edx\n"        // FEHashUpper
        "push 0x546265\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}

namespace CustomizationRestorations
{
    void Install()
    {
        // Hook at 0x54625C to append the special vinyl category to the menu
        injector::MakeJMP(0x54625C, (void*)VinylCategoryCodeCave, true);
    }
}
