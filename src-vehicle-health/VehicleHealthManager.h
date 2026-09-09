#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
#include <unordered_map>

struct VehicleHealthData
{
    float currentHealth = 100.0f;
    float maxHealth = 100.0f;
    float lagHealth = 100.0f;       // Smoothly trailing bar
    bool isDead = false;
    bool isPlayer = false;
    bool isOpponent = false;
    bool isTraffic = false;
    DWORD lastDamageTick = 0;
};

struct bVector3
{
    float x, y, z;
};

struct bMatrix4
{
    float m[4][4];
};

struct VehicleRenderEntry
{
    uintptr_t carPtr;
    bVector3 worldRoofPos;
    VehicleHealthData health;
};

namespace VehicleHealthManager
{
    void Init();
    void Update(float dt);
    void ResetAllHealth();
    void OnCollisionForce(void* thisBody, void* contactPoint, float force);
    bool IsCarDead(void* car);
    bool GetVehicleHealth(uintptr_t car, VehicleHealthData* outData);
    uintptr_t GetPlayerCar();
    bool IsCarActive(uintptr_t car);
    std::vector<VehicleRenderEntry> GetActiveVehiclesForRender();
    void DamagePlayerCar(float amount);
    void DisqualifyCarInRace(uintptr_t car);
    void ImmobilizeCar(uintptr_t car);
    bool IsInFreeRoam();
}

