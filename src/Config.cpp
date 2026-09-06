#include "Config.h"
#include "includes/IniReader.h"

PluginConfig g_Config;

void PluginConfig::Load(const char* iniPath)
{
    CIniReader ini(iniPath);

    restoreDebugCarCustomize = ini.ReadInteger("Restorations", "RestoreDebugCarCustomize", 1) != 0;
    restoreHiddenCameras     = ini.ReadInteger("Restorations", "RestoreHiddenCameras", 1) != 0;
    restoreUnusedRaceModes   = ini.ReadInteger("Restorations", "RestoreUnusedRaceModes", 1) != 0;
    restoreSpecialVinyls     = ini.ReadInteger("Restorations", "RestoreSpecialVinyls", 1) != 0;

    enableHotkeys            = ini.ReadInteger("Gameplay", "EnableDebugHotkeys", 1) != 0;
    hotkeyAutoDrive          = ini.ReadInteger("Hotkeys", "AutoDrive", 117); // F6
    hotkeyUnlockAll          = ini.ReadInteger("Hotkeys", "UnlockAll", 116); // F5
}
