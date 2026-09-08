#include "VehicleDamageRestorations.h"
#include "../GameAddresses.h"
#include "../Logger.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"
#include <cmath>
#include <algorithm>

extern "C" {
    uintptr_t s_CurrentCollisionBody = 0;
}

namespace VehicleDamageRestorations
{
    struct bVector3
    {
        float x, y, z;
    };

    struct bMatrix4
    {
        float m[4][4];
    };

    static float s_SimTime = 0.0f;

    // Helper to calculate damage stage (0..3) for each of the 6 parts
    // and pack into bits 16..27 of CarCollisionBody + 0xE0
    void UpdateDamageStages(uintptr_t collisionBody)
    {
        if (!collisionBody)
            return;

        uint32_t partStages = 0;
        float* damageArray = (float*)(collisionBody + 0x120);

        for (int i = 0; i < 6; ++i)
        {
            float dmg = damageArray[i];
            uint32_t stage = 0;
            if (dmg >= 2.0f)
                stage = 3;
            else if (dmg >= 1.0f)
                stage = 2;
            else if (dmg >= 0.25f)
                stage = 1;

            partStages |= (stage << (16 + i * 2));
        }

        uint32_t currentE0 = *(uint32_t*)(collisionBody + 0xE0);
        *(uint32_t*)(collisionBody + 0xE0) = (currentE0 & 0x0000FFFF) | partStages;
    }

    // 1. Hook CarPartDamage::UpdateDamage (0x00610CD0)
    // __thiscall: ecx = CarPartDamage* (located at CarCollisionBody + 0x100)
    void __fastcall Hooked_CarPartDamage_UpdateDamage(void* thisPartDamage, void* edx)
    {
        if (!thisPartDamage)
            return;

        uintptr_t collisionBody = (uintptr_t)thisPartDamage - 0x100;
        UpdateDamageStages(collisionBody);
    }

    // 2. Hook WobblingAnimationChannelDispatcher (0x00615280)
    // __thiscall: ecx = Vehicle*
    typedef void (__thiscall* WobbleDispatcherFn)(void* vehicle);
    static WobbleDispatcherFn s_OrigWobbleDispatcher = (WobbleDispatcherFn)0x00615280;

    void __fastcall Hooked_WobbleDispatcher(void* vehicle, void* edx)
    {
        if (vehicle)
        {
            uintptr_t collisionBody = *(uintptr_t*)((uintptr_t)vehicle + 0x958);
            if (collisionBody)
            {
                UpdateDamageStages(collisionBody);
            }
        }

        s_OrigWobbleDispatcher(vehicle);
    }

    // 3. Hook CarCollisionBody::AddDamageForce (0x00637C70)
    // __thiscall: ecx = CarCollisionBody*, args: (bVector3* contactPoint, float force)
    typedef void (__thiscall* AddDamageForceFn)(void* thisBody, bVector3* contactPoint, float force);
    static AddDamageForceFn s_OrigAddDamageForce = (AddDamageForceFn)0x00637C70;

    void __fastcall Hooked_AddDamageForce(void* thisBody, void* edx, bVector3* contactPoint, float force)
    {
        uintptr_t collisionBody = (uintptr_t)thisBody;
        if (!collisionBody || !contactPoint)
            return;

        // Ensure global damage flag is enabled
        *(uint8_t*)0x0089E7D2 = 1;

        // Responsive collision force delivery
        if (force >= 0.015f)
        {
            // Scale force for realistic 3-stage progression on crashes
            float scaledForce = force * 4.0f;
            float* dmg = (float*)(collisionBody + 0x120);

            // contactPoint coordinates: x: +front/-rear, y: +left/-right, z: +up/-down
            if (contactPoint->y > -0.25f) // Left side impact
            {
                if (contactPoint->x > -0.5f)
                {
                    dmg[0] = std::min(5.0f, dmg[0] + scaledForce * 0.45f); // Left Mirror
                    dmg[4] = std::min(5.0f, dmg[4] + scaledForce * 0.50f); // Left Door
                }
            }

            if (contactPoint->y < 0.25f) // Right side impact
            {
                if (contactPoint->x > -0.5f)
                {
                    dmg[1] = std::min(5.0f, dmg[1] + scaledForce * 0.45f); // Right Mirror
                    dmg[5] = std::min(5.0f, dmg[5] + scaledForce * 0.50f); // Right Door
                }
            }

            if (contactPoint->x < 0.2f) // Rear impact
            {
                dmg[3] = std::min(5.0f, dmg[3] + scaledForce * 0.65f); // Trunk
                dmg[2] = std::min(5.0f, dmg[2] + scaledForce * 0.45f); // Exhaust
                if (contactPoint->y > 0.0f)
                    dmg[4] = std::min(5.0f, dmg[4] + scaledForce * 0.25f);
                else
                    dmg[5] = std::min(5.0f, dmg[5] + scaledForce * 0.25f);
            }

            if (contactPoint->x > 0.0f) // Front impact
            {
                dmg[0] = std::min(5.0f, dmg[0] + scaledForce * 0.30f);
                dmg[1] = std::min(5.0f, dmg[1] + scaledForce * 0.30f);
                dmg[4] = std::min(5.0f, dmg[4] + scaledForce * 0.25f);
                dmg[5] = std::min(5.0f, dmg[5] + scaledForce * 0.25f);
            }

            UpdateDamageStages(collisionBody);

            Logger::Log("[DAMAGE] Crash: force=%.3f pos=(%.2f, %.2f, %.2f) | Dmg: M_L=%.2f M_R=%.2f Exh=%.2f Trk=%.2f D_L=%.2f D_R=%.2f",
                force, contactPoint->x, contactPoint->y, contactPoint->z,
                dmg[0], dmg[1], dmg[2], dmg[3], dmg[4], dmg[5]);
        }

        s_OrigAddDamageForce(thisBody, contactPoint, force);
    }

    // 4. Hook CarCollisionBody::ComputeDamageBitfield at 0x00633F08
    // Preserve part stages in bits 16..31 across body deformation recalculations
    typedef uint32_t (__thiscall* ComputeDamageBitfieldFn)(void* thisBody);
    static ComputeDamageBitfieldFn s_OrigComputeDamageBitfield = (ComputeDamageBitfieldFn)0x00610EA0;

    uint32_t __fastcall Hooked_ComputeDamageBitfield(void* thisBody, void* edx)
    {
        uint32_t bodyStages = s_OrigComputeDamageBitfield(thisBody);
        uint32_t currentE0 = *(uint32_t*)((uintptr_t)thisBody + 0xE0);
        return (bodyStages & 0x0000FFFF) | (currentE0 & 0xFFFF0000);
    }

    // 5. Code cave to capture CarCollisionBody* during CarRenderInfo::Render (at 0x0062109C)
    __attribute__((naked)) static void CaptureCurrentCollisionBodyCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "mov [esp + 0x180], eax\n"
            "mov _s_CurrentCollisionBody, eax\n"
            "push 0x006210A3\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    // 6. Hook bMatrix4::Rotate (0x005BAE90) for Trunk Lid and Left/Right Doors
    typedef void (__cdecl* RotateFn)(bMatrix4* out, bMatrix4* in, bVector3* angles);
    static RotateFn s_OrigRotate = (RotateFn)0x005BAE90;

    static void __cdecl TrunkRotateHook(bMatrix4* out, bMatrix4* in, bVector3* angles)
    {
        bVector3 modAngles = *angles;
        if (s_CurrentCollisionBody)
        {
            float trunkDmg = *(float*)(s_CurrentCollisionBody + 0x12C);
            if (trunkDmg >= 0.25f)
            {
                float stage = (trunkDmg >= 2.0f) ? 3.0f : ((trunkDmg >= 1.0f) ? 2.0f : 1.0f);
                // Flapping up and down on the pitch axis (X)
                float flap = (std::sin(s_SimTime * 18.0f) * 0.5f + 0.5f) * (0.055f * stage);
                modAngles.x += flap;
            }
        }
        s_OrigRotate(out, in, &modAngles);
    }

    static void __cdecl LeftDoorRotateHook(bMatrix4* out, bMatrix4* in, bVector3* angles)
    {
        bVector3 modAngles = *angles;
        if (s_CurrentCollisionBody)
        {
            float doorLDmg = *(float*)(s_CurrentCollisionBody + 0x130);
            if (doorLDmg >= 0.25f)
            {
                float stage = (doorLDmg >= 2.0f) ? 3.0f : ((doorLDmg >= 1.0f) ? 2.0f : 1.0f);
                // Flapping and vibrating on yaw axis (Y)
                float flap = (std::sin(s_SimTime * 15.0f) * 0.5f + 0.5f) * (0.045f * stage);
                modAngles.y += flap;
            }
        }
        s_OrigRotate(out, in, &modAngles);
    }

    static void __cdecl RightDoorRotateHook(bMatrix4* out, bMatrix4* in, bVector3* angles)
    {
        bVector3 modAngles = *angles;
        if (s_CurrentCollisionBody)
        {
            float doorRDmg = *(float*)(s_CurrentCollisionBody + 0x134);
            if (doorRDmg >= 0.25f)
            {
                float stage = (doorRDmg >= 2.0f) ? 3.0f : ((doorRDmg >= 1.0f) ? 2.0f : 1.0f);
                // Flapping and vibrating on yaw axis (Y)
                float flap = (std::sin(s_SimTime * 15.0f + 1.5f) * 0.5f + 0.5f) * (0.045f * stage);
                modAngles.y -= flap;
            }
        }
        s_OrigRotate(out, in, &modAngles);
    }

    // 7. Initialize table 0x8A0388 with non-zero part damage weights
    static void InitDamageWeightsTable()
    {
        typedef void (*InitDamageWeightsFn)();
        ((InitDamageWeightsFn)0x00610B00)();
        *(uint32_t*)0x008A1D04 = 1;

        float* table = (float*)0x008A0388;
        for (int cell = 0; cell < 24; ++cell)
        {
            float* w = &table[cell * 6];
            if (w[0] == 0.0f) w[0] = 0.50f; // MirrorL
            if (w[1] == 0.0f) w[1] = 0.50f; // MirrorR
            if (w[2] == 0.0f) w[2] = 0.40f; // Exhaust
            if (w[3] == 0.0f) w[3] = 0.60f; // Trunk
            if (w[4] == 0.0f) w[4] = 0.50f; // DoorL
            if (w[5] == 0.0f) w[5] = 0.50f; // DoorR
        }
    }

    void Install()
    {
        // 1. Enable all 6 parts in the CarDamagePartInfo table (0x00803698):
        for (int i = 0; i < 6; ++i)
        {
            injector::WriteMemory<uint32_t>(0x00803698 + i * 16, 1, true);
        }

        // 2. Enable the global engine damage system flags
        injector::WriteMemory<uint32_t>(0x00870CCC, 1, true);
        injector::WriteMemory<uint32_t>(0x008764E4, 1, true);
        *(uint8_t*)0x0089E7D2 = 1;

        // 3. Prevent graphic presets / quality clamps from disabling g_CarDamageEnable at 0x005BE6F7
        injector::MakeNOP(0x005BE6F7, 5, true);

        // 4. Force damage enabled on race load: replace 'setne al' with 'mov al, 1; nop' at 0x00540033
        injector::WriteMemory<uint8_t>(0x00540033, 0xB0, true);
        injector::WriteMemory<uint8_t>(0x00540034, 0x01, true);
        injector::WriteMemory<uint8_t>(0x00540035, 0x90, true);

        // 5. Force vehicle collision damage triggers in Collision::CheckForWildCollision (0x0059FC13)
        injector::WriteMemory<uint8_t>(0x0059FC13, 0xEB, true);

        // 6. Ensure impact force damage calculation in CarCollisionBody::AddDamageForce (0x00637C7A)
        injector::MakeNOP(0x00637C7A, 6, true);

        // 7. Hook AddDamageForce call at 0x00593C44 for responsive impact force delivery
        injector::MakeCALL(0x00593C44, (void*)Hooked_AddDamageForce, true);

        // 8. Hook CarPartDamage::UpdateDamage (0x00610CD0) to calculate and pack part stages into +0xE0
        injector::MakeJMP(0x00610CD0, (void*)Hooked_CarPartDamage_UpdateDamage, true);

        // 9. Hook WobblingAnimationChannelDispatcher (0x00615280) caller at 0x0063873D
        injector::MakeCALL(0x0063873D, (void*)Hooked_WobbleDispatcher, true);

        // 10. Hook ComputeDamageBitfield caller at 0x00633F08 to preserve part stages in +0xE0
        injector::MakeCALL(0x00633F08, (void*)Hooked_ComputeDamageBitfield, true);

        // 11. Fix mirror detachment thresholds in CarRenderInfo::Render:
        // Left Mirror (0x006264F6) and Right Mirror (0x00626A67) compare against 4.0f instead of 0.25f
        injector::WriteMemory<uint32_t>(0x006264F6, 0x00803940, true);
        injector::WriteMemory<uint32_t>(0x00626A67, 0x00803940, true);

        // 12. Fix IsPartDamaged (0x00615240) comparison threshold at 0x00615267:
        // Compare with 4.0f (broken off) so wobbling parts (0.25f..3.99f) remain rendered
        injector::WriteMemory<uint32_t>(0x00615267, 0x00803940, true);

        // 13. Fix disappearing parts in CarRenderInfo::Render at 0x00624456:
        // When IsPartDamaged returns true (dmg >= 4.0f), jump directly to 0x0062449E (drop mesh without malloc leak)
        injector::MakeJMP(0x00624456, (void*)0x0062449E, true);
        injector::MakeNOP(0x00624456 + 5, 13, true);

        // 14. Capture CarCollisionBody* during CarRenderInfo::Render at 0x0062109C
        injector::MakeJMP(0x0062109C, (void*)CaptureCurrentCollisionBodyCodeCave, true);
        injector::MakeNOP(0x0062109C + 5, 2, true);

        // 15. Hook door and trunk hinge rotation calls in CarRenderInfo::Render for dynamic flapping
        injector::MakeCALL(0x00623A46, (void*)TrunkRotateHook, true);
        injector::MakeCALL(0x00624170, (void*)LeftDoorRotateHook, true);
        injector::MakeCALL(0x0062417F, (void*)RightDoorRotateHook, true);

        // 16. Initialize collision weights table
        InitDamageWeightsTable();

        Logger::Log("[+] VehicleDamageRestorations: Restored 6 damage parts, collision force delivery, 3-stage wobbling animations, dynamic flapping hinges, and detachment at 4.0f threshold");
    }

    void Update(float dt)
    {
        s_SimTime += dt;
        *(uint8_t*)0x0089E7D2 = 1;
    }

    void CyclePlayerDamage()
    {
        void* player = *(void**)GameAddresses::PlayersByNumber;
        if (!player)
            return;

        void* car = *(void**)((uintptr_t)player + 4);
        if (!car)
            return;

        uintptr_t collisionBody = *(uintptr_t*)((uintptr_t)car + 0x958);
        if (!collisionBody)
            return;

        static int s_CycleStage = 0;
        s_CycleStage = (s_CycleStage + 1) % 5;

        float dmg = 0.0f;
        const char* stageName = "Repaired";
        switch (s_CycleStage)
        {
        case 0: dmg = 0.0f; stageName = "Repaired (Stage 0)"; break;
        case 1: dmg = 0.6f; stageName = "Light Wobble (Stage 1)"; break;
        case 2: dmg = 1.5f; stageName = "Medium Wobble (Stage 2)"; break;
        case 3: dmg = 2.8f; stageName = "Heavy Wobble (Stage 3)"; break;
        case 4: dmg = 4.5f; stageName = "Broken Off / Detached (Stage 4)"; break;
        }

        float* damageArray = (float*)(collisionBody + 0x120);
        for (int i = 0; i < 6; ++i)
        {
            damageArray[i] = dmg;
        }

        UpdateDamageStages(collisionBody);
        Logger::Log("[DAMAGE] F7 Hotkey: Set player vehicle damage to %.2f -> %s", dmg, stageName);
    }
}
