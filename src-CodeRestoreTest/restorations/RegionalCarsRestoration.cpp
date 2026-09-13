#include "RegionalCarsRestoration.h"
#include "../Config.h"
#include "../Logger.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"
#include <windows.h>
#include <cstdio>
#include <sys/stat.h>

// Regional car restoration for SPEED2.EXE v1.2 US.
// Adapted from NFSU2 Extra Options "Remove Region Car Lock" (Reference-Code/NFSU2ExOpts,
// ExtraOptionsStuff.h / CodeCaves.h), verified against this binary via IDA:
//
//   IsCarAvailableInCurrentBuildRegion (0x5022C0) excludes cars by scanning a per-region
//   table of 32-bit car-type IDs and returns false for any match:
//     - US build (GetBuildRegion() == 0): 0x7F7C48 "EUExclusiveCars"   = { 7 CORSA, 6 PEUGOT106 }
//     - EU/ROW build:                     0x7F7C50 "USExclusiveCars"  = { 5 CIVIC, 18 RSX }
//   Its only callers are UICareerCarLot::BuildCarList (call site 0x4DFA83) and
//   UIQRCarSelect::BuildCarList (call site 0x4E2374), which skip the car on false.
//
//   PickRandomCarsOfSomeSort (0x4FEAF0) draws random AI opponent cars from
//   RandomlyChooseableCarConfigsNorthAmerica (table at 0x7F6DA4); the element count is
//   loaded by "mov ebp, 0x1A" at 0x4FEB9C and the two slots at 0x7F6DBE/0x7F6DBF are
//   unused padding, so regional car types can be appended and the count extended.
//
// Differences to the Extra Options original (which requires BOTH CARS\CORSA and
// CARS\PEUGOT106 to exist and then applies all patches at once): each car's patches are
// applied only when its own model data exists on disc, so installs without the Corsa
// data (US retail discs) still get the Peugeot 106. The region-check bypass hooks are
// only installed when every car excluded by the current region's table has data, since
// the bypass would otherwise make data-less cars selectable. The low-byte 0xFF write
// alone (entry becomes 0x0000FFxx, unmatchable by any car type) is sufficient for the
// car list path. The CIVIC/RSX entries (0x7F7C50/0x7F7C54) are written as in the
// original; they are only consulted on EU/ROW builds and are no-ops on US copies.
//
// Note: file checks use CWD-relative "..\CARS\..." paths, matching Extra Options - the
// Ultimate ASI Loader sets the current directory to the scripts folder before loading
// ASI plugins, so ".." resolves to the game root. If NFSU2ExtraOptions is also
// installed with UnlockRegionals there, these patches are idempotent duplicates.

namespace
{
    // EUExclusiveCars (US build exclusion table): { 7 = OPEL CORSA, 6 = PEUGEOT 106 }
    constexpr uintptr_t AddrCorsaExclusionEntry       = 0x7F7C48;
    constexpr uintptr_t AddrPeugeot106ExclusionEntry  = 0x7F7C4C;
    // USExclusiveCars (EU/ROW build exclusion table): { 5 = CIVIC, 18 = RSX }
    constexpr uintptr_t AddrCivicExclusionEntry       = 0x7F7C50;
    constexpr uintptr_t AddrRsxExclusionEntry         = 0x7F7C54;

    constexpr uintptr_t AddrCareerCarLotRegionCheck   = 0x4DFA83; // call IsCarAvailableInCurrentBuildRegion in UICareerCarLot::BuildCarList
    constexpr uintptr_t AddrQRCarSelectRegionCheck    = 0x4E2374; // call IsCarAvailableInCurrentBuildRegion in UIQRCarSelect::BuildCarList

    constexpr uintptr_t AddrRandomAICarConfigCount    = 0x4FEB9D; // immediate of "mov ebp, 0x1A" in PickRandomCarsOfSomeSort
    constexpr uintptr_t AddrRandomAICarConfigTableEnd = 0x7F6DBE; // first free slots after RandomlyChooseableCarConfigsNorthAmerica

    constexpr uint8_t CarTypeOpelCorsa  = 7;
    constexpr uint8_t CarTypePeugeot106 = 6;

    constexpr uint8_t NativeRandomCarCount = 0x1A;
    constexpr uint8_t MaxRandomCarCount    = 0x1C; // Extra Options: "Extend array by 2"

    // From NFSU2 Extra Options Helpers.h
    inline bool DoesFileExist(const char* path)
    {
        struct stat buffer;
        return (stat(path, &buffer) == 0);
    }

    // The retail US disc ships the car folder as "CARS\PEUGOT"; Extra Options expects "PEUGOT106".
    inline bool Peugeot106DataExists()
    {
        return DoesFileExist("..\\CARS\\PEUGOT106\\TEXTURES.BIN")
            && DoesFileExist("..\\CARS\\PEUGOT106\\GEOMETRY.BIN")
            && DoesFileExist("..\\CARS\\PEUGOT\\TEXTURES.BIN")
            && DoesFileExist("..\\CARS\\PEUGOT\\GEOMETRY.BIN");
    }

    inline bool OpelCorsaDataExists()
    {
        return DoesFileExist("..\\CARS\\CORSA\\TEXTURES.BIN")
            && DoesFileExist("..\\CARS\\CORSA\\GEOMETRY.BIN");
    }

    bool __cdecl RegionLockHook(int /*CarID*/)
    {
        return 1;
    }
}

namespace RegionalCarsRestoration
{
    void Install()
    {
        const bool peugeotData = Peugeot106DataExists();
        const bool corsaData = OpelCorsaDataExists();

        if (!peugeotData && !corsaData)
        {
            Logger::Log("[-] Regional Cars: no regional car model data found (CARS/PEUGOT, CARS/CORSA), skipping");
            return;
        }

        // Unlock 106 and Corsa for Car List, EUExclusiveCars
        if (peugeotData)
        {
            injector::WriteMemory<uint8_t>(AddrPeugeot106ExclusionEntry, 0xFF, true); // PEUGOT106
            Logger::Log("[+] Regional Cars: Peugeot 106 unlocked for Car List (0x7F7C4C)");
        }
        if (corsaData)
        {
            injector::WriteMemory<uint8_t>(AddrCorsaExclusionEntry, 0xFF, true); // CORSA
            Logger::Log("[+] Regional Cars: Opel Corsa unlocked for Car List (0x7F7C48)");
        }

        // Fix Civic and RSX for other regions, USExclusiveCars
        injector::WriteMemory<uint8_t>(AddrCivicExclusionEntry, 0xFF, true); // CIVIC
        injector::WriteMemory<uint8_t>(AddrRsxExclusionEntry, 0xFF, true);   // RSX

        // Hook region lock to return always true - only safe when every car the current
        // build region excludes (US build: CORSA + PEUGOT106) has model data on disc,
        // otherwise a data-less car could be selected and crash the game.
        if (peugeotData && corsaData)
        {
            injector::MakeCALL(AddrCareerCarLotRegionCheck, (void*)RegionLockHook, true); // UICareerCarLot::BuildCarList
            injector::MakeCALL(AddrQRCarSelectRegionCheck, (void*)RegionLockHook, true);  // UIQRCarSelect::BuildCarList
            Logger::Log("[+] Regional Cars: IsCarAvailableInCurrentBuildRegion bypassed in both car list builders");
        }

        // Unlock them as AI opponents
        uint8_t appended = 0;
        uint8_t* appendSlot = (uint8_t*)AddrRandomAICarConfigTableEnd;

        if (corsaData)
            appendSlot[appended++] = CarTypeOpelCorsa;
        if (peugeotData)
            appendSlot[appended++] = CarTypePeugeot106;

        if (appended > 0 && NativeRandomCarCount + appended <= MaxRandomCarCount)
        {
            injector::WriteMemory<uint8_t>(AddrRandomAICarConfigCount, NativeRandomCarCount + appended, true); // PickRandomCarsOfSomeSort
            Logger::Log("[+] Regional Cars: %d regional car(s) added to the random AI opponent pool", appended);
        }
    }
}
