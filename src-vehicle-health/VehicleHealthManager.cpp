#include "VehicleHealthManager.h"
#include "Config.h"
#include "Logger.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"
#include <algorithm>
#include <cmath>

namespace VehicleHealthManager
{
    static std::unordered_map<uintptr_t, VehicleHealthData> s_HealthMap;
    static uint32_t s_LastGameFlow = 0;
    static bool s_IsInCareerFreeRoam = false;

    extern "C" void __cdecl VehicleHealthManager_Reset()
    {
        ResetAllHealth();
    }

    extern "C" void __cdecl VehicleHealthManager_OnStartRace()
    {
        s_IsInCareerFreeRoam = false;
        VehicleHealthLogger::Log("[+] StartRace: Competitive race started, Career FreeRoam mode cleared.");
        ResetAllHealth();
    }

    extern "C" void __cdecl VehicleHealthManager_OnStartCareerFreeRoam()
    {
        s_IsInCareerFreeRoam = true;
        VehicleHealthLogger::Log("[+] StartCareerFreeRoam: Career FreeRoam mode active.");
        ResetAllHealth();
    }

    bool IsInFreeRoam()
    {
        if (s_IsInCareerFreeRoam)
            return true;

        int trackId = *(int*)0x0089E7A0;
        if (trackId == 4000)
            return true;

        void* race = *(void**)0x00890118;
        if (!race || (uintptr_t)race < 0x00010000)
            return true;

        int numRacers = *(int*)((uintptr_t)race + 0x24);
        if (numRacers <= 1)
            return true;

        return false;
    }

    void ResetAllHealth()
    {
        VehicleHealthLogger::Log("[+] ResetAllHealth: Clearing health map (%zu tracked vehicles, FreeRoam=%d).",
                                 s_HealthMap.size(), IsInFreeRoam() ? 1 : 0);
        s_HealthMap.clear();
    }

    uintptr_t GetPlayerCar()
    {
        uintptr_t world = *(uintptr_t*)0x00890080;
        if (world && world > 0x00400000 && world < 0x7FFFFFFF)
        {
            int totalCars = *(int*)(world + 0x10);
            if (totalCars > 0 && totalCars <= 64)
            {
                for (int i = 0; i < totalCars; ++i)
                {
                    uintptr_t car = *(uintptr_t*)(world + 0x1C + i * 4);
                    if (!car || car < 0x00400000 || car > 0x7FFFFFFF) continue;

                    uintptr_t driverInfo = *(uintptr_t*)(car + 0x14);
                    if (driverInfo && driverInfo > 0x00400000 && driverInfo < 0x7FFFFFFF)
                    {
                        int driverType = *(int*)(driverInfo + 4);
                        if (driverType == 1) // 1 = DRIVER_HUMAN (The local player)
                            return car;
                    }
                }
            }
        }

        // Fallback: Player 0 car pointer at 0x008900AC
        uintptr_t playerPtr = *(uintptr_t*)0x008900AC;
        if (playerPtr && playerPtr > 0x00400000 && playerPtr < 0x7FFFFFFF)
        {
            uintptr_t c = *(uintptr_t*)(playerPtr + 4);
            if (c && c > 0x00400000 && c < 0x7FFFFFFF)
                return c;
        }

        return 0;
    }

    bool IsCarActive(uintptr_t car)
    {
        if (!car || car < 0x00400000 || car > 0x7FFFFFFF)
            return false;

        // The local player car is always active
        if (car == GetPlayerCar())
            return true;

        // 1. Check active flag at Car + 0x550 (mIsActive in SPEED2.EXE)
        // In SPEED2.EXE, 1 = active / spawned in world, 0 = inactive / despawned / pooled
        uint8_t isActive = *(uint8_t*)(car + 0x550);
        if (isActive == 0)
            return false;

        // 2. Check Z coordinate (Car + 0x68)
        // Despawned/unspawned vehicles are parked off-world at Z = -123456.0f (0x8025b8 in SPEED2.EXE)
        float cz = *(float*)(car + 0x68);
        if (std::isnan(cz) || std::isinf(cz) || std::fabs(cz - (-123456.0f)) < 1.0f || std::fabs(cz) > 50000.0f)
            return false;

        // 3. For traffic cars, check TrafficAI at Car + 0x2C if present
        // In SPEED2.EXE at 0x4099d9, trafficAI + 0x77D is 1 when deactivated/sleeping/despawned
        uintptr_t trafficAI = *(uintptr_t*)(car + 0x2C);
        if (trafficAI && trafficAI >= 0x00400000 && trafficAI < 0x7FFFFFFF)
        {
            uint8_t inactiveFlag = *(uint8_t*)(trafficAI + 0x77D);
            if (inactiveFlag != 0)
                return false;
        }

        return true;
    }

    static bool IsValidCar(uintptr_t car)
    {
        if (!car || car < 0x00400000 || car > 0x7FFFFFFF) return false;
        uintptr_t world = *(uintptr_t*)0x00890080;
        if (!world || world < 0x00400000 || world > 0x7FFFFFFF) return false;

        int totalCars = *(int*)(world + 0x10);
        if (totalCars <= 0 || totalCars > 64) return false;

        for (int i = 0; i < totalCars; ++i)
        {
            uintptr_t c = *(uintptr_t*)(world + 0x1C + i * 4);
            if (c == car)
            {
                return IsCarActive(car);
            }
        }
        return false;
    }


    void DisqualifyCarInRace(uintptr_t car)
    {
        if (!car || car < 0x00010000) return;
        if (IsInFreeRoam())
        {
            VehicleHealthLogger::Log("[Race] DisqualifyCarInRace skipped: currently in Free Roam.");
            return;
        }

        void* race = *(void**)0x00890118;
        if (!race || (uintptr_t)race < 0x00010000)
        {
            VehicleHealthLogger::Log("[Race] DisqualifyCarInRace: No active race (TheRace pointer at 0x00890118 is invalid).");
            return;
        }

        int numRacers = *(int*)((uintptr_t)race + 0x24);
        VehicleHealthLogger::Log("[Race] DisqualifyCarInRace for Car 0x%08lX (race=%p, numRacers=%d)",
                                 (unsigned long)car, race, numRacers);

        if (numRacers <= 0 || numRacers > 8)
        {
            VehicleHealthLogger::Log("[Race] DisqualifyCarInRace: numRacers (%d) invalid.", numRacers);
            return;
        }

        uintptr_t playerCar = GetPlayerCar();
        bool isPlayer = (car == playerCar);

        uintptr_t racingCar = *(uintptr_t*)(car + 0x1C);
        int racerIndex = -1;

        // Match car in race->racingCars array at 0xBD0
        for (int i = 0; i < numRacers; ++i)
        {
            uintptr_t rc = *(uintptr_t*)((uintptr_t)race + 0xBD0 + i * 4);
            if (rc && rc >= 0x00400000 && rc < 0x7FFFFFFF)
            {
                if (rc == racingCar || (car && *(uintptr_t*)rc == car))
                {
                    racerIndex = i;
                    racingCar = rc;
                    break;
                }
            }
        }

        // If player car not matched by pointer, player is always index 0 in TheRace
        if (racerIndex == -1 && isPlayer)
        {
            racerIndex = 0;
            if (!racingCar)
                racingCar = *(uintptr_t*)((uintptr_t)race + 0xBD0);
        }

        if (racerIndex < 0 || racerIndex >= numRacers)
        {
            VehicleHealthLogger::Log("[Race] DisqualifyCarInRace: Could not find racerIndex for Car 0x%08lX (racingCar=0x%08lX).",
                                     (unsigned long)car, (unsigned long)racingCar);
            return;
        }

        if (racingCar && racingCar >= 0x00400000 && racingCar < 0x7FFFFFFF)
        {
            int16_t currentStatus = *(int16_t*)(racingCar + 0x10);
            if (currentStatus != 0)
            {
                VehicleHealthLogger::Log("[Race] Racer #%d already has finish status %d, skipping.", racerIndex, currentStatus);
                return;
            }
        }

        uint32_t raceType = *(uint32_t*)0x0089E7B0;
        int reason = g_HealthConfig.disqualifyReason;
        if (raceType == 3) // Drag race: RACING_CAR_DRAG_BLOWNENGINE
        {
            reason = 8;
        }

        // Determine the last place rank (numRacers, or the lowest unoccupied rank from numRacers down to 1)
        int lastPlace = (numRacers > 0) ? numRacers : 4;
        for (int pos = lastPlace; pos >= 1; --pos)
        {
            bool taken = false;
            for (int i = 0; i < numRacers; ++i)
            {
                uintptr_t rc = *(uintptr_t*)((uintptr_t)race + 0xBD0 + i * 4);
                if (rc && rc >= 0x00400000 && rc < 0x7FFFFFFF && rc != racingCar)
                {
                    int16_t status = *(int16_t*)(rc + 0x10);
                    uint8_t p = *(uint8_t*)(rc + 0x0C);
                    if (status != 0 && p == pos)
                    {
                        taken = true;
                        break;
                    }
                }
            }
            if (!taken)
            {
                lastPlace = pos;
                break;
            }
        }

        // Explicitly set finish position fields on RacingCar so FinishCar won't retain the car's current active race position (e.g. 1st place)
        if (racingCar && racingCar >= 0x00400000 && racingCar < 0x7FFFFFFF)
        {
            *(uint8_t*)(racingCar + 0x0C) = (uint8_t)lastPlace;
            *(int16_t*)(racingCar + 0x0E) = (int16_t)lastPlace;
            *(int16_t*)(racingCar + 0x14) = (int16_t)lastPlace;
        }

        typedef void (__thiscall* FinishCarFn)(void* thisRace, int carIndex, int finishPos, int a3, int reason);
        FinishCarFn fnFinishCar = (FinishCarFn)0x00606CB0;
        fnFinishCar(race, racerIndex, lastPlace, 0, reason);

        VehicleHealthLogger::Log("[Race] Disqualified racer #%d (Car 0x%08lX, isPlayer=%d) with reason %d (%s) at rank %d/%d!",
                                 racerIndex, (unsigned long)car, isPlayer ? 1 : 0, reason,
                                 (reason == 8 ? "BLOWN ENGINE" : (reason == 3 ? "KNOCKED OUT" : "TOTALLED")),
                                 lastPlace, numRacers);

        // If this is the local human player, notify game state machine that player race has ended
        if (isPlayer)
        {
            *(uint8_t*)0x0083ABCC = (uint8_t)reason;
            *(uint32_t*)0x0083ABC4 = 1;
            VehicleHealthLogger::Log("[Race] Set player race finished state: 0x0083ABC4 = 1, 0x0083ABCC = %d (Rank: %d/%d).",
                                     reason, lastPlace, numRacers);
        }
    }

    void OnCollisionForce(void* thisBody, void* contactPoint, float force)
    {
        if (!g_HealthConfig.enableHealthSystem || !thisBody)
            return;

        if (std::isnan(force) || std::isinf(force) || force < g_HealthConfig.minForceThreshold)
            return;

        // In NFSU2 CarCollisionBody + 0x130 points directly to Car*
        uintptr_t collisionBody = (uintptr_t)thisBody;
        if (collisionBody < 0x00400000 || collisionBody > 0x7FFFFFFF)
            return;

        uintptr_t car = *(uintptr_t*)(collisionBody + 0x130);
        if (!IsValidCar(car))
            return;

        // Auto-register car if not yet tracked
        auto it = s_HealthMap.find(car);
        if (it == s_HealthMap.end())
        {
            VehicleHealthData data;
            data.currentHealth = g_HealthConfig.maxHealth;
            data.maxHealth = g_HealthConfig.maxHealth;
            data.lagHealth = g_HealthConfig.maxHealth;
            data.isDead = false;
            data.lastDamageTick = 0;

            uintptr_t playerCar = GetPlayerCar();
            uintptr_t driverInfo = *(uintptr_t*)(car + 0x14);
            int driverType = (driverInfo && driverInfo > 0x00400000) ? *(int*)(driverInfo + 4) : 0;
            data.isPlayer   = (car == playerCar) || (driverType == 1);
            data.isOpponent = !data.isPlayer && (driverType == 2);
            data.isTraffic  = !data.isPlayer && !data.isOpponent;

            it = s_HealthMap.emplace(car, data).first;
        }

        if (it->second.isDead)
            return;

        // In Free Roam, player car health does not decrease
        if (it->second.isPlayer && g_HealthConfig.disablePlayerHealthInFreeRoam && IsInFreeRoam())
            return;

        // Cooldown debounce: prevent multi-tick contacts during a single collision from draining all health
        DWORD now = GetTickCount();
        if (now - it->second.lastDamageTick < 120) // 120ms debounce
            return;
        it->second.lastDamageTick = now;

        // Balanced damage calculation:
        // Multiplier = 0.40f * config (makes 10-force tap deal ~4 HP, 40-force crash deal ~16 HP, 100-force crash deal ~40 HP)
        float effectiveForce = force - g_HealthConfig.minForceThreshold;
        if (effectiveForce <= 0.0f) return;

        float damage = effectiveForce * g_HealthConfig.damageMultiplier * 0.40f;
        if (damage < 1.0f) damage = 1.0f;
        if (damage > it->second.maxHealth) damage = it->second.maxHealth;

        it->second.currentHealth = std::max(0.0f, it->second.currentHealth - damage);
        VehicleHealthLogger::Log("[Collision] Car 0x%08lX (%s) hit with force %.4f -> Damage: %.1f, New HP: %.1f/%.1f",
                                 (unsigned long)car,
                                 it->second.isPlayer ? "PLAYER" : (it->second.isOpponent ? "OPPONENT" : "TRAFFIC"),
                                 force, damage, it->second.currentHealth, it->second.maxHealth);

        if (it->second.currentHealth <= 0.0f)
        {
            it->second.currentHealth = 0.0f;
            it->second.isDead = true;
            VehicleHealthLogger::Log("[!] Car 0x%08lX (%s) DESTROYED (0 HP)! Disabling drivetrain.",
                                     (unsigned long)car,
                                     it->second.isPlayer ? "PLAYER" : (it->second.isOpponent ? "OPPONENT" : "TRAFFIC"));

            if (g_HealthConfig.disableVehicleOnDeath)
            {
                uintptr_t driver = *(uintptr_t*)(car + 0x30);
                if (driver)
                {
                    *(float*)(driver + 0x20C) = 0.0f; // Forward throttle
                    *(float*)(driver + 0x210) = 0.0f; // Reverse throttle / Brake
                    *(float*)(driver + 0x214) = 0.0f; // Handbrake
                }
                *(int*)(car + 0x4D0) = 0; // Neutral gear
            }

            // Disqualify car in race status if enabled (applies to AI and Player)
            if (g_HealthConfig.disqualifyOnDeath)
            {
                DisqualifyCarInRace(car);
            }
        }
    }

    bool IsCarDead(void* car)
    {
        if (!car) return false;
        auto it = s_HealthMap.find((uintptr_t)car);
        if (it != s_HealthMap.end())
        {
            return it->second.isDead;
        }
        return false;
    }

    bool GetVehicleHealth(uintptr_t car, VehicleHealthData* outData)
    {
        if (!car || !outData) return false;
        auto it = s_HealthMap.find(car);
        if (it != s_HealthMap.end())
        {
            *outData = it->second;
            return true;
        }
        return false;
    }

    void DamagePlayerCar(float amount)
    {
        if (std::isnan(amount) || std::isinf(amount) || amount <= 0.0f)
            return;

        if (g_HealthConfig.disablePlayerHealthInFreeRoam && IsInFreeRoam())
        {
            VehicleHealthLogger::Log("[Damage] DamagePlayerCar ignored: Player is in Free Roam.");
            return;
        }

        uintptr_t playerCar = GetPlayerCar();
        if (!playerCar)
        {
            VehicleHealthLogger::Log("[!] DamagePlayerCar: Player car pointer not found.");
            return;
        }

        auto it = s_HealthMap.find(playerCar);
        if (it == s_HealthMap.end())
        {
            VehicleHealthData data;
            data.currentHealth = g_HealthConfig.maxHealth;
            data.maxHealth = g_HealthConfig.maxHealth;
            data.lagHealth = g_HealthConfig.maxHealth;
            data.isDead = false;
            data.isPlayer = true;
            data.lastDamageTick = 0;
            it = s_HealthMap.emplace(playerCar, data).first;
        }

        it->second.currentHealth = std::max(0.0f, it->second.currentHealth - amount);
        VehicleHealthLogger::Log("[+] DamagePlayerCar: Player car 0x%08lX took %.1f damage -> HP: %.1f / %.1f",
                                 (unsigned long)playerCar, amount, it->second.currentHealth, it->second.maxHealth);

        if (it->second.currentHealth <= 0.0f)
        {
            it->second.currentHealth = 0.0f;
            it->second.isDead = true;
            VehicleHealthLogger::Log("[!] Player car DESTROYED (0 HP)! Engine / drivetrain disabled.");

            uintptr_t driver = *(uintptr_t*)(playerCar + 0x30);
            if (driver)
            {
                *(float*)(driver + 0x20C) = 0.0f; // Forward throttle
                *(float*)(driver + 0x210) = 0.0f; // Reverse throttle / Brake
                *(float*)(driver + 0x214) = 0.0f; // Handbrake
            }
            *(int*)(playerCar + 0x4D0) = 0; // Neutral gear

            if (g_HealthConfig.disqualifyOnDeath)
            {
                DisqualifyCarInRace(playerCar);
            }
        }
    }

    std::vector<VehicleRenderEntry> GetActiveVehiclesForRender()
    {
        std::vector<VehicleRenderEntry> result;
        uintptr_t world = *(uintptr_t*)0x00890080;
        if (!world || world < 0x00400000 || world > 0x7FFFFFFF) return result;

        int totalCars = *(int*)(world + 0x10);
        if (totalCars <= 0 || totalCars > 64) return result;

        int trafficStartIndex = *(int*)(world + 0x18);
        uintptr_t playerCar = GetPlayerCar();

        for (int i = 0; i < totalCars; ++i)
        {
            uintptr_t car = *(uintptr_t*)(world + 0x1C + i * 4);
            if (!car || car < 0x00400000 || car > 0x7FFFFFFF) continue;

            // Immediately skip and purge inactive / despawned / pooled vehicles
            if (!IsCarActive(car))
            {
                if (car != playerCar)
                {
                    s_HealthMap.erase(car);
                }
                continue;
            }

            auto it = s_HealthMap.find(car);
            if (it == s_HealthMap.end())
            {
                VehicleHealthData data;
                data.currentHealth = g_HealthConfig.maxHealth;
                data.maxHealth = g_HealthConfig.maxHealth;
                data.lagHealth = g_HealthConfig.maxHealth;
                data.isDead = false;
                data.lastDamageTick = 0;

                uintptr_t driverInfo = *(uintptr_t*)(car + 0x14);
                int driverType = (driverInfo && driverInfo > 0x00400000) ? *(int*)(driverInfo + 4) : 0;
                data.isPlayer   = (car == playerCar) || (driverType == 1);
                data.isOpponent = !data.isPlayer && (driverType == 2 || (trafficStartIndex > 0 && i < trafficStartIndex));
                data.isTraffic  = !data.isPlayer && !data.isOpponent;

                it = s_HealthMap.emplace(car, data).first;
            }
            else
            {
                // Ensure isPlayer is strictly kept in sync with local player car
                if (car == playerCar && !it->second.isPlayer)
                {
                    it->second.isPlayer = true;
                    it->second.isOpponent = false;
                    it->second.isTraffic = false;
                }
            }

            // Filter by configuration options
            if (it->second.isPlayer && !g_HealthConfig.showPlayerBar)
                continue;
            if (it->second.isOpponent && !g_HealthConfig.showOpponentBars)
                continue;
            if (it->second.isTraffic && !g_HealthConfig.showTrafficBars)
                continue;

            VehicleRenderEntry entry;
            entry.carPtr = car;

            // Base position coordinates directly from Car + 0x60, 0x64, 0x68
            float cx = *(float*)(car + 0x60);
            float cy = *(float*)(car + 0x64);
            float cz = *(float*)(car + 0x68);

            entry.worldRoofPos = { cx, cy, cz + g_HealthConfig.healthBarHeightOffset };
            entry.health = it->second;

            result.push_back(entry);
        }

        return result;
    }

    void Update(float dt)
    {
        uint32_t currentFlow = *(uint32_t*)0x008654A4;
        // If transitioning into gameplay (state 6), reset all health
        if (currentFlow == 6 && s_LastGameFlow != 6)
        {
            VehicleHealthLogger::Log("[+] GameFlow transitioned to 6 (Gameplay). Resetting all vehicle health.");
            ResetAllHealth();
        }
        s_LastGameFlow = currentFlow;

        if (currentFlow != 6)
            return;

        // Hotkeys with debouncing for Wine compatibility
        if (g_HealthConfig.enableHotkeys)
        {
            static bool s_F8WasDown = false;
            bool f8Down = (GetAsyncKeyState(g_HealthConfig.hotkeyRepairAll) & 0x8000) != 0;
            if (f8Down && !s_F8WasDown)
            {
                VehicleHealthLogger::Log("[Hotkey] F8 pressed: Full repair on all vehicles.");
                ResetAllHealth();
            }
            s_F8WasDown = f8Down;

            static bool s_F9WasDown = false;
            bool f9Down = (GetAsyncKeyState(g_HealthConfig.hotkeyDamagePlayer) & 0x8000) != 0;
            if (f9Down && !s_F9WasDown)
            {
                VehicleHealthLogger::Log("[Hotkey] F9 pressed: Damaging player vehicle by 25%%.");
                DamagePlayerCar(25.0f);
            }
            s_F9WasDown = f9Down;
        }

        // Maintain health states and smooth lag bar
        uintptr_t world = *(uintptr_t*)0x00890080;
        if (!world || world < 0x00400000 || world > 0x7FFFFFFF) return;

        int totalCars = *(int*)(world + 0x10);
        if (totalCars <= 0 || totalCars > 64) return;

        uintptr_t playerCar = GetPlayerCar();

        for (int i = 0; i < totalCars; ++i)
        {
            uintptr_t car = *(uintptr_t*)(world + 0x1C + i * 4);
            if (!car || car < 0x00400000 || car > 0x7FFFFFFF) continue;

            // Immediately purge any vehicle that despawned
            if (!IsCarActive(car))
            {
                if (car != playerCar)
                {
                    s_HealthMap.erase(car);
                }
                continue;
            }

            auto it = s_HealthMap.find(car);
            if (it != s_HealthMap.end())
            {
                if (it->second.isPlayer && g_HealthConfig.disablePlayerHealthInFreeRoam && IsInFreeRoam())
                {
                    it->second.currentHealth = it->second.maxHealth;
                    it->second.lagHealth = it->second.maxHealth;
                    it->second.isDead = false;
                }

                // Smooth damage lag bar dropping towards current health
                if (it->second.lagHealth > it->second.currentHealth)
                {
                    it->second.lagHealth -= dt * 35.0f;
                    if (it->second.lagHealth < it->second.currentHealth)
                    {
                        it->second.lagHealth = it->second.currentHealth;
                    }
                }
                else if (it->second.lagHealth < it->second.currentHealth)
                {
                    it->second.lagHealth = it->second.currentHealth;
                }

                // If dead, continuously enforce zero throttle, zero reverse, and Neutral gear
                if (it->second.isDead && g_HealthConfig.disableVehicleOnDeath)
                {
                    uintptr_t driver = *(uintptr_t*)(car + 0x30);
                    if (driver && driver >= 0x00400000 && driver < 0x7FFFFFFF)
                    {
                        *(float*)(driver + 0x20C) = 0.0f; // Forward throttle = 0
                        *(float*)(driver + 0x210) = 0.0f; // Reverse throttle / Brake = 0
                        *(float*)(driver + 0x214) = 0.0f; // Handbrake = 0
                    }
                    *(int*)(car + 0x4D0) = 0; // Neutral gear
                }
            }
        }

        // Periodically purge despawned vehicles (e.g. ambient traffic in Free Roam) to prevent memory growth
        static int s_PurgeCounter = 0;
        if (++s_PurgeCounter >= 30)
        {
            s_PurgeCounter = 0;
            for (auto mapIt = s_HealthMap.begin(); mapIt != s_HealthMap.end(); )
            {
                if (!mapIt->second.isPlayer && (!IsValidCar(mapIt->first) || !IsCarActive(mapIt->first)))
                {
                    mapIt = s_HealthMap.erase(mapIt);
                }
                else
                {
                    ++mapIt;
                }
            }
        }
    }

    // -------------------------------------------------------------
    // Code Caves & Hooks
    // -------------------------------------------------------------

    extern "C" void __cdecl VehicleHealthManager_OnCollision(void* thisBody, void* contactPoint, float force)
    {
        OnCollisionForce(thisBody, contactPoint, force);
    }

    // Naked Hook for CarCollisionBody::AddDamage call at 0x005A0BC4 (ApplyImpulse)
    __attribute__((naked)) static void Hooked_CarCollisionBody_AddDamage()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "pushad\n"
            // [esp + 0x20] = RetAddr (0x005A0BC9)
            // [esp + 0x24] = contactPoint* (bVector3*)
            // [esp + 0x28] = force (float)
            "push dword ptr [esp + 0x28]\n" // force
            "push dword ptr [esp + 0x28]\n" // contactPoint
            "push ecx\n"                    // thisBody (CarCollisionBody*)
            "call _VehicleHealthManager_OnCollision\n"
            "add esp, 12\n"
            "popad\n"
            "push 0x00593BF0\n"              // Position-independent jump to original CarCollisionBody::AddDamage
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    extern "C" void __cdecl VehicleHealthManager_OnDelegateInput(void* mover)
    {
        if (!mover || (uintptr_t)mover < 0x00400000 || (uintptr_t)mover > 0x7FFFFFFF) return;
        uintptr_t car = *(uintptr_t*)((uintptr_t)mover + 0x5C);
        if (!car || car < 0x00400000 || car > 0x7FFFFFFF) return;

        if (IsCarDead((void*)car))
        {
            uintptr_t driver = *(uintptr_t*)((uintptr_t)mover + 4);
            if (driver && driver >= 0x00400000 && driver < 0x7FFFFFFF)
            {
                *(float*)(driver + 0x20C) = 0.0f;   // Force forward throttle = 0.0f
                *(float*)(driver + 0x210) = 0.0f;   // Force reverse throttle / brake = 0.0f
                *(float*)(driver + 0x214) = 0.0f;   // Force handbrake = 0.0f
            }
            *(int*)(car + 0x4D0) = 0;               // Force gear = Neutral
        }
    }

    // Naked Code Cave at PhysicsMover::DelegateDriverInput (0x005ABBF0)
    __attribute__((naked)) static void DelegateDriverInputCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "pushad\n"
            "push ecx\n" // mover (PhysicsMover*)
            "call _VehicleHealthManager_OnDelegateInput\n"
            "add esp, 4\n"
            "popad\n"

            // Replicate overwritten preamble:
            "sub esp, 8\n"
            "push esi\n"
            "mov esi, ecx\n"
            "push 0x005ABBF6\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    // Lifecycle Code Caves for Resetting Health & Tracking Game Modes
    __attribute__((naked)) static void StartRaceCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "pushad\n"
            "call _VehicleHealthManager_OnStartRace\n"
            "popad\n"
            "sub esp, 0x28\n"
            "mov ecx, 0x0089E7A0\n"
            "push 0x0053FC28\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    __attribute__((naked)) static void StartCareerFreeRoamCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "pushad\n"
            "call _VehicleHealthManager_OnStartCareerFreeRoam\n"
            "popad\n"
            "push ebp\n"
            "mov ebp, esp\n"
            "and esp, 0xFFFFFFF0\n"
            "push 0x005404A6\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    __attribute__((naked)) static void InitForRaceCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "pushad\n"
            "call _VehicleHealthManager_Reset\n"
            "popad\n"
            "push -1\n"
            "push 0x0077A0EC\n"
            "push 0x0060A687\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    __attribute__((naked)) static void CleanupAfterRaceCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "pushad\n"
            "call _VehicleHealthManager_Reset\n"
            "popad\n"
            "push ebx\n"
            "push esi\n"
            "push edi\n"
            "mov edi, ecx\n"
            "push 0x0060A9A5\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    void Init()
    {
        if (!g_HealthConfig.enableHealthSystem)
            return;

        // 1. Hook Call to CarCollisionBody::AddDamage inside ApplyImpulse at 0x005A0BC4
        VehicleHealthLogger::Log("  [Hook] Installing CarCollisionBody::AddDamage hook at 0x005A0BC4...");
        injector::MakeCALL(0x005A0BC4, (void*)Hooked_CarCollisionBody_AddDamage, true);

        // 2. Hook Driver Input Delegation at 0x005ABBF0 (kills throttle & reverse when dead)
        VehicleHealthLogger::Log("  [Hook] Installing DelegateDriverInput code cave at 0x005ABBF0...");
        injector::MakeJMP(0x005ABBF0, (void*)DelegateDriverInputCodeCave, true);

        // 3. Hook Race Start: StartRace at 0x0053FC20
        VehicleHealthLogger::Log("  [Hook] Installing StartRace code cave at 0x0053FC20...");
        injector::MakeJMP(0x0053FC20, (void*)StartRaceCodeCave, true);

        // 4. Hook Career FreeRoam: StartCareerFreeRoam at 0x005404A0
        VehicleHealthLogger::Log("  [Hook] Installing StartCareerFreeRoam code cave at 0x005404A0...");
        injector::MakeJMP(0x005404A0, (void*)StartCareerFreeRoamCodeCave, true);

        // 5. Hook InitForRace at 0x0060A680
        VehicleHealthLogger::Log("  [Hook] Installing InitForRace code cave at 0x0060A680...");
        injector::MakeJMP(0x0060A680, (void*)InitForRaceCodeCave, true);

        // 6. Hook CleanupAfterRace at 0x0060A9A0
        VehicleHealthLogger::Log("  [Hook] Installing CleanupAfterRace code cave at 0x0060A9A0...");
        injector::MakeJMP(0x0060A9A0, (void*)CleanupAfterRaceCodeCave, true);
    }
}
