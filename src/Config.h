#pragma once

struct PluginConfig
{
    // Restorations
    bool restoreDebugCarCustomize = true;
    bool restoreHiddenCameras     = true;
    bool restoreUnusedRaceModes   = true;
    bool restoreSpecialVinyls     = true;

    // Hotkeys
    int  hotkeyAutoDrive          = 117; // F6
    int  hotkeyUnlockAll          = 116; // F5
    bool enableHotkeys            = true;

    void Load(const char* iniPath);
};

extern PluginConfig g_Config;
