#pragma once

#include <cstdint>

namespace BurnoutRestorations
{
    void Init();
    void Update(float dt);

    // Trick IDs matching the PS2 Demo / GameCube trick table
    enum eBurnoutTrick
    {
        TRICK_NONE = -1,
        TRICK_CW_DONUT = 0,
        TRICK_CCW_DONUT = 1,
        TRICK_S_CURVE = 2,
        TRICK_S_MIRROR = 3,
        TRICK_FIGURE_8 = 4
    };

    struct BurnoutState
    {
        bool isBurnoutActive;
        float currentYawRate;
        float accumulatedCWAngle;
        float accumulatedCCWAngle;
        float totalBurnoutDistance;
        float sessionScore;
        int comboMultiplier;
        eBurnoutTrick lastTrick;
        float timeSinceLastTrick;
    };

    const BurnoutState& GetState();
}
