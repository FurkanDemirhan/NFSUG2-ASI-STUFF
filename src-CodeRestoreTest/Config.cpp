#include "Config.h"
#include "includes/IniReader.h"
#include <cstdio>

PluginConfig g_Config;

static void ParsePivot(const char* value, float out[3])
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (value && sscanf(value, "%f , %f , %f", &x, &y, &z) == 3)
    {
        out[0] = x; out[1] = y; out[2] = z;
    }
}

void PluginConfig::Load(const char* iniPath)
{
    const char* filename = iniPath;
    if (filename)
    {
        if (strncmp(filename, "scripts/", 8) == 0) filename += 8;
        else if (strncmp(filename, "scripts\\", 8) == 0) filename += 8;
    }
    CIniReader ini(filename && filename[0] ? filename : "NFSU2CodeRestoration.ini");

    restoreDebugCarCustomize     = ini.ReadInteger("Restorations", "RestoreDebugCarCustomize", 1) != 0;
    restoreHiddenCameras         = ini.ReadInteger("Restorations", "RestoreHiddenCameras", 1) != 0;
    restoreUnusedRaceModes       = ini.ReadInteger("Restorations", "RestoreUnusedRaceModes", 1) != 0;
    restoreSprintDriftOpponents  = ini.ReadInteger("Restorations", "RestoreSprintDriftOpponents", 1) != 0;
    restoreURLLapController      = ini.ReadInteger("Restorations", "RestoreURLLapController", 1) != 0;
    restoreRestartRaceInAllModes = ini.ReadInteger("Restorations", "RestoreRestartRaceInAllModes", 1) != 0;
    restoreSpecialVinyls         = ini.ReadInteger("Restorations", "RestoreSpecialVinyls", 1) != 0;
    restoreBurnoutMode           = ini.ReadInteger("Restorations", "RestoreBurnoutMode", 1) != 0;
    restoreLapKOInQR             = ini.ReadInteger("Restorations", "RestoreLapKOInQR", 1) != 0;
    restoreVehicleDamage         = ini.ReadInteger("Gameplay", "RestoreVehicleDamage", ini.ReadInteger("Restorations", "RestoreVehicleDamage", 1)) != 0;

    unlockRegionalCars           = ini.ReadInteger("Cars", "UnlockRegionalCars", 1) != 0;

    restoreEncounterOpponentCleanup = ini.ReadInteger("DemoCareer", "RestoreEncounterOpponentCleanup", 1) != 0;

    wobbleScale = ini.ReadFloat("VehicleDamage", "WobbleScale", 1.0f);
    if (wobbleScale < 0.0f) wobbleScale = 0.0f;
    if (wobbleScale > 3.0f) wobbleScale = 3.0f;

    ParsePivot(ini.ReadString("VehicleDamage", "PivotMirrorL", "0.0, 0.0, 0.0"),  partPivots[0]);
    ParsePivot(ini.ReadString("VehicleDamage", "PivotMirrorR", "0.0, 0.0, 0.0"), partPivots[1]);
    ParsePivot(ini.ReadString("VehicleDamage", "PivotExhaust", "0.0, 0.0, 0.0"),   partPivots[2]);
    ParsePivot(ini.ReadString("VehicleDamage", "PivotTrunk",   "-1.05, 0.0, 0.85"),  partPivots[3]);
    ParsePivot(ini.ReadString("VehicleDamage", "PivotDoorL",   "0.68, 0.82, 0.22"), partPivots[4]);
    ParsePivot(ini.ReadString("VehicleDamage", "PivotDoorR",   "0.68, -0.82, 0.22"), partPivots[5]);

    fixDisappearingWheels        = ini.ReadInteger("Fixes", "FixDisappearingWheels", 1) != 0;
    fixAudioPathCrash            = ini.ReadInteger("Fixes", "FixAudioPathCrash", 1) != 0;
    fixMissingBarriersCrash      = ini.ReadInteger("Fixes", "FixMissingBarriersCrash", 1) != 0;
    removeLockedAreaBarriers     = ini.ReadInteger("Fixes", "RemoveLockedAreaBarriers", 1) != 0;
    removeRaceBarriers           = ini.ReadInteger("Gameplay", "RemoveRaceBarriers", 0) != 0;
    unlockAnyTrackInAnyMode      = ini.ReadInteger("Menu", "UnlockAnyTrackInAnyMode", 1) != 0;

    enableDebugWorldCamera       = ini.ReadInteger("Debugging", "EnableDebugWorldCamera", 0) != 0;

    enableHotkeys                = ini.ReadInteger("Gameplay", "EnableDebugHotkeys", 1) != 0;
    hotkeyAutoDrive              = ini.ReadInteger("Hotkeys", "AutoDrive", 117); // F6
    hotkeyUnlockAll              = ini.ReadInteger("Hotkeys", "UnlockAll", 116); // F5
    hotkeyDamageCycle            = ini.ReadInteger("Hotkeys", "DamageCycle", 118); // F7
}
