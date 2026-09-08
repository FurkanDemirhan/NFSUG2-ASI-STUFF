#include "Config.h"
#include "includes/IniReader.h"

PluginConfig g_Config;

void PluginConfig::Load(const char* iniPath)
{
    CIniReader ini(iniPath);

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
