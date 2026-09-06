#pragma once

struct PluginConfig
{
    // Restorations
    bool restoreDebugCarCustomize      = true;
    bool restoreHiddenCameras          = true;
    bool restoreUnusedRaceModes        = true;
    bool restoreSprintDriftOpponents   = true;
    bool restoreURLLapController       = true;
    bool restoreRestartRaceInAllModes  = true;
    bool restoreSpecialVinyls          = true;
    bool restoreBurnoutMode            = true;
    bool restoreLapKOInQR              = true;

    // Fixes & Debugging
    bool fixDisappearingWheels         = true;
    bool enableDebugWorldCamera        = false;

    // Track & Barrier Options
    bool fixMissingBarriersCrash       = true;
    bool unlockAnyTrackInAnyMode       = true;
    bool removeRaceBarriers            = false;
    bool removeLockedAreaBarriers      = true;

    // Hotkeys
    int  hotkeyAutoDrive               = 117; // F6
    int  hotkeyUnlockAll               = 116; // F5
    bool enableHotkeys                 = true;

    void Load(const char* iniPath);
};

extern PluginConfig g_Config;
