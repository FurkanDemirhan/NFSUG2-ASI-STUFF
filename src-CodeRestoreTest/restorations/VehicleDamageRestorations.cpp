#include "VehicleDamageRestorations.h"
#include "../GameAddresses.h"
#include "../Logger.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"
#include <cmath>
#include <algorithm>

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

    // Function pointer for eMesh::Render (0x005C5930)
    typedef void (__thiscall* MeshRenderFn)(void* eViewPlat, void* model, bMatrix4* mtx, void* lightCtx, uint32_t flags, uint32_t flags2);
    static MeshRenderFn s_OrigMeshRender = (MeshRenderFn)0x005C5930;

    // Dedicated per-part wobbled matrix buffers (prevents any cross-part matrix collisions)
    static bMatrix4 s_PartWobbleMatrix[6];

    // Helper to calculate damage stage (0..3) for each of the 6 parts
    // and pack into bits 16..27 of CarCollisionBody + 0xE0 for window glass texture cracking
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

    // 2. Hook CarCollisionBody::AddDamageForce (0x00637C70)
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

    // 3. Hook CarCollisionBody::ComputeDamageBitfield at 0x00633F08
    // Preserve part stages in bits 16..31 across body deformation recalculations
    typedef uint32_t (__thiscall* ComputeDamageBitfieldFn)(void* thisBody);
    static ComputeDamageBitfieldFn s_OrigComputeDamageBitfield = (ComputeDamageBitfieldFn)0x00610EA0;

    uint32_t __fastcall Hooked_ComputeDamageBitfield(void* thisBody, void* edx)
    {
        uint32_t bodyStages = s_OrigComputeDamageBitfield(thisBody);
        uint32_t currentE0 = *(uint32_t*)((uintptr_t)thisBody + 0xE0);
        return (bodyStages & 0x0000FFFF) | (currentE0 & 0xFFFF0000);
    }

    struct PivotPoint {
        float x, y, z;
    };

    // Car-space pivot positions (hinges / mounting points)
    // In NFSU2 car coordinates: +X is Front, -X is Rear, +Y is Left, -Y is Right, +Z is Up
    static const PivotPoint s_PartPivots[6] = {
        {  0.50f,  0.88f,  0.55f }, // Part 0: Left Mirror (A-pillar base)
        {  0.50f, -0.88f,  0.55f }, // Part 1: Right Mirror (A-pillar base)
        { -1.65f, -0.38f,  0.08f }, // Part 2: Exhaust (rear underbody hanger)
        { -0.75f,  0.00f,  0.58f }, // Part 3: Trunk (top hinge near rear window)
        {  0.68f,  0.82f,  0.22f }, // Part 4: Left Door (front A-pillar hinge)
        {  0.68f, -0.82f,  0.22f }  // Part 5: Right Door (front A-pillar hinge)
    };

    // Row-major matrix multiplication: out = A * B
    static void MultiplyMatrices(bMatrix4* out, const bMatrix4* A, const bMatrix4* B)
    {
        bMatrix4 temp;
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                temp.m[i][j] = A->m[i][0] * B->m[0][j] +
                               A->m[i][1] * B->m[1][j] +
                               A->m[i][2] * B->m[2][j] +
                               A->m[i][3] * B->m[3][j];
            }
        }
        *out = temp;
    }

    // Build a 4x4 transformation matrix that rotates by Euler angles (yaw-pitch-roll)
    // around the part's specific hinge pivot point P, keeping the hinge invariant (P * M = P).
    static void BuildPivotWobbleMatrix(int partIndex, const bVector3* angles, bMatrix4* out)
    {
        float cx = std::cos(angles->x), sx = std::sin(angles->x);
        float cy = std::cos(angles->y), sy = std::sin(angles->y);
        float cz = std::cos(angles->z), sz = std::sin(angles->z);

        // Combined 3D rotation R = Rz * Ry * Rx (row-vector convention: v_rot = v * R)
        float r00 = cz * cy;
        float r01 = cz * sy * sx + sz * cx;
        float r02 = -cz * sy * cx + sz * sx;

        float r10 = -sz * cy;
        float r11 = -sz * sy * sx + cz * cx;
        float r12 = sz * sy * cx + cz * sx;

        float r20 = sy;
        float r21 = -cy * sx;
        float r22 = cy * cx;

        const PivotPoint& p = s_PartPivots[partIndex];
        // Invariant hinge pivot: T = P - P * R
        float tx = p.x - (p.x * r00 + p.y * r10 + p.z * r20);
        float ty = p.y - (p.x * r01 + p.y * r11 + p.z * r21);
        float tz = p.z - (p.x * r02 + p.y * r12 + p.z * r22);

        out->m[0][0] = r00; out->m[0][1] = r01; out->m[0][2] = r02; out->m[0][3] = 0.0f;
        out->m[1][0] = r10; out->m[1][1] = r11; out->m[1][2] = r12; out->m[1][3] = 0.0f;
        out->m[2][0] = r20; out->m[2][1] = r21; out->m[2][2] = r22; out->m[2][3] = 0.0f;
        out->m[3][0] = tx;  out->m[3][1] = ty;  out->m[3][2] = tz;  out->m[3][3] = 1.0f;
    }

    // 4. Physical Wobble Angle Calculation for all 6 vehicle damage parts
    void ComputePartWobbleAngles(int partIndex, float stage, bVector3* outAngles)
    {
        outAngles->x = 0.0f;
        outAngles->y = 0.0f;
        outAngles->z = 0.0f;

        if (stage <= 0.0f) return;

        float t = s_SimTime;

        switch (partIndex)
        {
        case 0: // Left Mirror
        {
            // Rapid high-frequency vibration/jitter around mirror base mount
            outAngles->x = std::sin(t * 28.0f) * (0.012f * stage);
            outAngles->y = std::cos(t * 23.0f) * (0.010f * stage);
            outAngles->z = std::sin(t * 32.0f) * (0.014f * stage);
            break;
        }
        case 1: // Right Mirror
        {
            outAngles->x = std::sin(t * 29.0f + 1.1f) * (0.012f * stage);
            outAngles->y = -std::cos(t * 24.0f + 1.1f) * (0.010f * stage);
            outAngles->z = -std::sin(t * 33.0f + 1.1f) * (0.014f * stage);
            break;
        }
        case 2: // Exhaust
        {
            // Broken rubber hanger: drops down slightly on pitch and shakes laterally
            outAngles->y = -((std::sin(t * 18.0f) * 0.5f + 0.5f) * (0.012f * stage) + 0.008f * stage);
            outAngles->z = std::sin(t * 25.0f) * (0.008f * stage);
            break;
        }
        case 3: // Trunk Lid
        {
            // Latch popped: bounces upward on pitch (Y) axis around top hinge
            outAngles->y = (std::sin(t * 15.0f) * 0.5f + 0.5f) * (0.015f * stage) + 0.006f * stage;
            outAngles->x = std::sin(t * 24.0f) * (0.003f * stage);
            break;
        }
        case 4: // Left Door
        {
            // Unlatched: ajar and fluttering open outward on yaw (Z) axis (negative direction)
            // Rear latch rattles against frame; front hinge stays 100% attached to A-pillar
            outAngles->z = -((std::sin(t * 16.0f) * 0.5f + 0.5f) * (0.012f * stage) + 0.008f * stage);
            outAngles->x = std::sin(t * 28.0f) * (0.003f * stage);
            break;
        }
        case 5: // Right Door
        {
            // Unlatched: ajar and fluttering open outward on yaw (Z) axis (positive direction)
            outAngles->z = ((std::sin(t * 16.0f + 0.7f) * 0.5f + 0.5f) * (0.012f * stage) + 0.008f * stage);
            outAngles->x = std::sin(t * 28.0f + 0.7f) * (0.003f * stage);
            break;
        }
        default:
            break;
        }
    }

    // 5. State and evaluation for breakable parts during CarRenderInfo::Render
    extern "C" {
        uintptr_t s_CurrentCollisionBody = 0;
        bMatrix4* s_ActivePartWobbleMatrix = nullptr;
    }

    extern "C" int EvaluatePartDamageAtRender(int partIndex, uintptr_t carDamage, bMatrix4* inMatrix)
    {
        s_ActivePartWobbleMatrix = nullptr;

        if (!carDamage || partIndex < 2 || partIndex > 5 || !inMatrix)
            return 0;

        float* dmgArray = (float*)(carDamage + 0x120);
        float dmg = dmgArray[partIndex];

        // Detachment / Break-off threshold: 4.0f
        if (dmg >= 4.0f)
        {
            return 2; // Detached! Skip rendering!
        }

        // Wobbling threshold: 0.25f
        if (dmg >= 0.25f)
        {
            float stage = (dmg >= 2.0f) ? 3.0f : ((dmg >= 1.0f) ? 2.0f : 1.0f);
            bVector3 angles;
            ComputePartWobbleAngles(partIndex, stage, &angles);

            bMatrix4 wobbleMtx;
            BuildPivotWobbleMatrix(partIndex, &angles, &wobbleMtx);
            MultiplyMatrices(&s_PartWobbleMatrix[partIndex], &wobbleMtx, inMatrix);

            s_ActivePartWobbleMatrix = &s_PartWobbleMatrix[partIndex];
            return 1; // Wobbling
        }

        return 0; // Intact
    }

    // Naked code cave at 0x00624441: replaces gutted check and malloc leak
    // Preserves ebx register completely so no car or wheel transforms are corrupted!
    __attribute__((naked)) static void PartDamageWobbleCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "push ebx\n"                           // arg 2: inMatrix (ebx)
            "push dword ptr [esp + 0x180 + 4]\n"   // arg 1: carDamage
            "push dword ptr [esp + 0x64 + 8]\n"    // arg 0: partIndex
            "call _EvaluatePartDamageAtRender\n"
            "add esp, 12\n"
            "cmp eax, 2\n"
            "je 1f\n"                              // if detached, skip mesh rendering
            // Intact or wobbling: ebx is untouched, proceed to normal mesh rendering
            "push 0x0062446A\n"
            "ret\n"
            "1:\n"
            "push 0x0062449E\n"                    // detached: skip mesh rendering cleanly
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    // Hook the mesh render call at 0x00624499 for breakable parts
    // Substitutes s_ActivePartWobbleMatrix if wobbling, then immediately resets it
    void __thiscall Hooked_BreakablePartMeshRender(void* eView, void* model, bMatrix4* mtx, void* lightCtx, uint32_t flags, uint32_t flags2)
    {
        bMatrix4* actualMtx = s_ActivePartWobbleMatrix ? s_ActivePartWobbleMatrix : mtx;
        s_ActivePartWobbleMatrix = nullptr;
        s_OrigMeshRender(eView, model, actualMtx, lightCtx, flags, flags2);
    }

    // 6. Code caves to capture/zero CarDamage* during CarRenderInfo::Render
    __attribute__((naked)) static void CaptureCurrentCollisionBodyCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "mov [esp + 0x180], eax\n"
            "mov _s_CurrentCollisionBody, eax\n"
            "mov dword ptr _s_ActivePartWobbleMatrix, 0\n"
            "push 0x006210A3\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    __attribute__((naked)) static void ZeroCurrentCollisionBodyCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "mov dword ptr [esp + 0x180], 0\n"
            "mov dword ptr _s_CurrentCollisionBody, 0\n"
            "mov dword ptr _s_ActivePartWobbleMatrix, 0\n"
            "push 0x00621092\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    // 7. Left Mirror and Right Mirror mesh render hooks (pivoted around mirror base)
    void __thiscall Hooked_RenderLeftMirrorMesh(void* eViewPlat, void* model, bMatrix4* mtx, void* lightCtx, uint32_t flags, uint32_t flags2)
    {
        bMatrix4* renderMtx = mtx;
        if (s_CurrentCollisionBody && mtx)
        {
            float dmg = *(float*)(s_CurrentCollisionBody + 0x120);
            if (dmg >= 0.25f && dmg < 4.0f)
            {
                float stage = (dmg >= 2.0f) ? 3.0f : ((dmg >= 1.0f) ? 2.0f : 1.0f);
                bVector3 angles;
                ComputePartWobbleAngles(0, stage, &angles);

                bMatrix4 wobbleMtx;
                BuildPivotWobbleMatrix(0, &angles, &wobbleMtx);
                MultiplyMatrices(&s_PartWobbleMatrix[0], &wobbleMtx, mtx);
                renderMtx = &s_PartWobbleMatrix[0];
            }
        }
        s_OrigMeshRender(eViewPlat, model, renderMtx, lightCtx, flags, flags2);
    }

    void __thiscall Hooked_RenderRightMirrorMesh(void* eViewPlat, void* model, bMatrix4* mtx, void* lightCtx, uint32_t flags, uint32_t flags2)
    {
        bMatrix4* renderMtx = mtx;
        if (s_CurrentCollisionBody && mtx)
        {
            float dmg = *(float*)(s_CurrentCollisionBody + 0x124);
            if (dmg >= 0.25f && dmg < 4.0f)
            {
                float stage = (dmg >= 2.0f) ? 3.0f : ((dmg >= 1.0f) ? 2.0f : 1.0f);
                bVector3 angles;
                ComputePartWobbleAngles(1, stage, &angles);

                bMatrix4 wobbleMtx;
                BuildPivotWobbleMatrix(1, &angles, &wobbleMtx);
                MultiplyMatrices(&s_PartWobbleMatrix[1], &wobbleMtx, mtx);
                renderMtx = &s_PartWobbleMatrix[1];
            }
        }
        s_OrigMeshRender(eViewPlat, model, renderMtx, lightCtx, flags, flags2);
    }

    // 8. Initialize table 0x8A0388 with non-zero part damage weights
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

        // 8. Hook CarPartDamage::UpdateDamage (0x00610CD0) to calculate and pack part stages into +0xE0 (for window cracking textures)
        injector::MakeJMP(0x00610CD0, (void*)Hooked_CarPartDamage_UpdateDamage, true);

        // 9. Hook ComputeDamageBitfield caller at 0x00633F08 to preserve part stages in +0xE0
        injector::MakeCALL(0x00633F08, (void*)Hooked_ComputeDamageBitfield, true);

        // 10. Capture and zero CarDamage* during CarRenderInfo::Render
        injector::MakeJMP(0x00621087, (void*)ZeroCurrentCollisionBodyCodeCave, true);
        injector::MakeNOP(0x00621087 + 5, 6, true);

        injector::MakeJMP(0x0062109C, (void*)CaptureCurrentCollisionBodyCodeCave, true);
        injector::MakeNOP(0x0062109C + 5, 2, true);

        // 11. Hook General Breakable Parts in CarRenderInfo::Render (0x00624441)
        // Intercept the check to dynamically wobble meshes at stages 1..3 and detach at 4.0f
        injector::MakeJMP(0x00624441, (void*)PartDamageWobbleCodeCave, true);
        injector::MakeNOP(0x00624441 + 5, 36, true); // 0x62446A - 0x624446 = 36 bytes

        // 12. Hook mesh render call at 0x00624499 to apply active wobble matrix safely without modifying ebx
        injector::MakeCALL(0x00624499, (void*)Hooked_BreakablePartMeshRender, true);

        // 13. Fix Left Mirror detachment threshold (0x006264F6) to compare against 4.0f (0x00803940)
        injector::WriteMemory<uint32_t>(0x006264F6, 0x00803940, true);

        // 13. Hook Left Mirror mesh render call at 0x006269F0 for dynamic wobbling
        injector::MakeCALL(0x006269F0, (void*)Hooked_RenderLeftMirrorMesh, true);

        // 14. Fix Right Mirror detachment threshold (0x00626A67) to compare against 4.0f (0x00803940)
        injector::WriteMemory<uint32_t>(0x00626A67, 0x00803940, true);

        // 15. Hook Right Mirror mesh render call at 0x00626FED for dynamic wobbling
        injector::MakeCALL(0x00626FED, (void*)Hooked_RenderRightMirrorMesh, true);

        // 16. Initialize collision weights table
        InitDamageWeightsTable();

        Logger::Log("[+] VehicleDamageRestorations: Restored 6 damage parts, collision force delivery, 3-stage dynamic part wobbling, and detachment at 4.0f threshold");
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
