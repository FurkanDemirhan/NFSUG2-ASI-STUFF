#include "Config.h"
#include "../includes/IniReader.h"

VehicleHealthConfig g_HealthConfig;

void VehicleHealthConfig::Load(const std::string& iniPath)
{
    CIniReader ini(iniPath.c_str());

    enableHealthSystem    = ini.ReadBoolean("General", "EnableHealthSystem", true);
    displayHealthBars     = ini.ReadBoolean("Visuals", "DisplayHealthBars", true);
    showPlayerBar         = ini.ReadBoolean("Visuals", "ShowPlayerBar", true);
    showOpponentBars      = ini.ReadBoolean("Visuals", "ShowOpponentBars", true);
    showTrafficBars       = ini.ReadBoolean("Visuals", "ShowTrafficBars", true);
    disableVehicleOnDeath = ini.ReadBoolean("Gameplay", "DisableVehicleOnDeath", true);
    disqualifyOnDeath     = ini.ReadBoolean("Gameplay", "DisqualifyOnDeath", true);
    disablePlayerHealthInFreeRoam = ini.ReadBoolean("Gameplay", "DisablePlayerHealthInFreeRoam", true);
    disqualifyReason      = ini.ReadInteger("Gameplay", "DisqualifyReason", 3);

    maxHealth             = ini.ReadFloat("Gameplay", "MaxHealth", 100.0f);
    damageMultiplier      = ini.ReadFloat("Gameplay", "DamageMultiplier", 1.0f);
    minForceThreshold     = ini.ReadFloat("Gameplay", "MinForceThreshold", 0.50f);

    healthBarHeightOffset  = ini.ReadFloat("Visuals", "HealthBarHeightOffset", 1.85f);
    healthBarForwardOffset = ini.ReadFloat("Visuals", "HealthBarForwardOffset", 0.0f);
    healthBarWidth         = ini.ReadInteger("Visuals", "HealthBarWidth", 100);
    healthBarHeight        = ini.ReadInteger("Visuals", "HealthBarHeight", 10);

    enableHotkeys         = ini.ReadBoolean("Hotkeys", "EnableHotkeys", true);
    hotkeyRepairAll       = ini.ReadInteger("Hotkeys", "HotkeyRepairAll", VK_F8);
    hotkeyDamagePlayer    = ini.ReadInteger("Hotkeys", "HotkeyDamagePlayer", VK_F9);

    // Sanitize and clamp values to prevent divide-by-zero or graphical glitches
    if (maxHealth <= 0.0f) maxHealth = 100.0f;
    if (damageMultiplier < 0.0f) damageMultiplier = 1.0f;
    if (minForceThreshold < 0.0f) minForceThreshold = 0.50f;
    if (healthBarWidth < 20) healthBarWidth = 100;
    if (healthBarHeight < 2) healthBarHeight = 10;
}
