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
}
