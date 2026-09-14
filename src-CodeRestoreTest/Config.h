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
    bool restoreVehicleDamage          = true;

    // Regional / Cut Cars
    bool unlockRegionalCars            = true;

    // PS2 Demo Career Behavior
    bool restoreEncounterOpponentCleanup = true;

    // Fixes & Debugging
    bool fixDisappearingWheels         = true;
    bool fixAudioPathCrash             = true;
    bool enableDebugWorldCamera        = false;

    // Track & Barrier Options
    bool fixMissingBarriersCrash       = true;
    bool unlockAnyTrackInAnyMode       = true;
    bool removeRaceBarriers            = false;
    bool removeLockedAreaBarriers      = true;

    // Hotkeys
    int  hotkeyAutoDrive               = 117; // F6
    int  hotkeyUnlockAll               = 116; // F5
    int  hotkeyDamageCycle             = 118; // F7
    bool enableHotkeys                 = true;

    // Vehicle Damage tuning
    float wobbleScale                  = 0.7f;
    float partPivots[6][3] = {
        {  0.00f,  0.00f,  0.00f },  // Part 0: Left Mirror (0 = vibrate in place)
        {  0.00f,  0.00f,  0.00f },  // Part 1: Right Mirror (0 = vibrate in place)
        {  0.00f,  0.00f,  0.00f },  // Part 2: Exhaust (0 = rotate in place)
        {  -1.05f,  0.00f,  0.85f }, // Part 3: Trunk lid hinge (car-matrix path only)
        {  0.68f,  0.82f,  0.22f },  // Part 4: Left Door hinge
        {  0.68f, -0.82f,  0.22f }   // Part 5: Right Door hinge
    };

    void Load(const char* iniPath);
};

extern PluginConfig g_Config;
