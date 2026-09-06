#include "RaceModeRestorations.h"
#include "../GameAddresses.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"

namespace RaceModeRestorations
{
    void Install()
    {
        // 1. Restore Outrun mode selection in Quick Race menu
        injector::MakeNOP(0x4B2F3E, 2, true); // UIQRModeSelect::Setup

        // 2. Restore track selection screen for Free Run (Mode 9)
        injector::MakeNOP(0x4B2AE3, 2, true);

        // 3. Restore track selection for Outrun (prevents forced randomization)
        injector::MakeNOP(0x4B2AA0, 2, true);
        injector::MakeNOP(0x5268F8, 2, true); // RaceStarter::SetupOutrun
    }
}
