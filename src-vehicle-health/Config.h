#pragma once
#include <windows.h>
#include <string>

struct VehicleHealthConfig
{
    bool enableHealthSystem = true;
    bool displayHealthBars = true;
    bool showPlayerBar = true;
    bool showOpponentBars = true;
    bool showTrafficBars = true;
    bool disableVehicleOnDeath = true;
    bool disqualifyOnDeath = true;
    bool disablePlayerHealthInFreeRoam = true;
    int disqualifyReason = 3; // 3 = RACING_CAR_KNOCKED_OUT, 8 = RACING_CAR_DRAG_BLOWNENGINE, 9 = RACING_CAR_DRAG_TOTALLED

    float maxHealth = 100.0f;
    float damageMultiplier = 1.0f;
    float minForceThreshold = 0.50f;

    float healthBarHeightOffset = 1.85f;
    float healthBarForwardOffset = 0.0f;
    int healthBarWidth = 100;
    int healthBarHeight = 10;

    bool enableHotkeys = true;
    int hotkeyRepairAll = VK_F8;
    int hotkeyDamagePlayer = VK_F9;

    void Load(const std::string& iniPath);
};

extern VehicleHealthConfig g_HealthConfig;
