#include "CameraRestorations.h"
#include "../GameAddresses.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"

// Restores hidden camera POV modes (Driver, Bumper, Hood, Orbit, Drift)
static int __cdecl GetPOVTypeFromPlayerCamera(int a1)
{
    switch (a1)
    {
    case 0:  return 2; // Bumper
    case 1:  return 4; // Close Chase
    case 2:  return 1; // Far Chase
    case 4:  return 5; // Drift POV
    case 5:  return 3; // Driver / Cockpit
    case 3:
    default: return 0;
    }
}

namespace CameraRestorations
{
    void Install()
    {
        // Increase maximum selectable camera angles to 5 in PlayerSettings::ScrollDriveCam
        injector::WriteMemory<int>(0x50254E, 5, true);
        injector::WriteMemory<int>(0x502575, 5, true);

        // Hook GetPOVTypeFromPlayerCamera to enable all POV types
        injector::MakeJMP(0x502500, (void*)GetPOVTypeFromPlayerCamera, true);
    }
}
