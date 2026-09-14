#include "VehicleDamageRestorations.h"
#include "../GameAddresses.h"
#include "../Config.h"
#include "../Logger.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"
#include <cmath>
#include <algorithm>

// Vehicle part damage restoration (SPEED2.EXE v1.2 US, cross-referenced with the PS2 Demo).
//
// Engine layout (all verified by disassembly):
//   - Car + 0x958                     = CarDamage*
//   - CarDamage + 0x100               = embedded CarPartDamage
//   - CarDamage + 0x120 + part*4      = per-part damage floats (0 .. ~4.0)
//   - CarDamagePartInfo table 0x803698, 6 entries x 0x10:
//       +0x00 enable flag (ALL ZERO in vanilla - the master kill-switch)
//       +0x0C render node id {32,32,16,10,17,18} used by CarRenderInfo::Render's part lookup
//   - Stage thresholds 0x803934 = {0.25, 1.0, 2.0}, detach 0x803940 = 4.0
//     (identical to the PS2 Demo's tables at 0x446190)
//   - Damage grid 0x8A0388: 4x3x2 cells x 6 part weights (FindDamageSector 0x610D30)
//   - CarRenderInfo::Render (0x620AE0) is UNROLLED PER PART and consults CarDamage* from
//     [esp+0x180] in several blocks:
//       0x622FCC  door damage flags (parts 4/5)
//       0x624404  generic part block, per-part matrix from an array (index at [esp+0x6C])
//       0x624441  generic part block, car-world matrix (ebx), part index at [esp+0x64]
//       0x6264E7  left mirror,  0x626A58 right mirror,  0x626F29 mirror/hood variant
//     All of them end in the shared eViewPlatInterface::Render call at 0x624499 (or the
//     dedicated mirror calls). Vanilla damaged-part flow mallocs 0x40 bytes and skips the
//     render (parts vanish + leak). The PS2 Demo drove wobble via per-part animation
//     channels (0x4461A0 = {45..49}; retail's orphaned copy 0x803944 = {47..51}).
//
// This module re-enables the engine's own damage pipeline and restores wobble by
// substituting the matrix passed to eMesh::Render with a pivot-invariant wobble matrix.
// The wobble is evaluated at each block's entry and COMPOSED AT THE RENDER CALL
// (wobble * actual-matrix), so both the car-matrix path and the per-part-matrix path
// wobble, and the hooks stay independent of each block's matrix space.

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
    static int s_LogHits = 0;
    static int s_DiagEvals = 0;
    static int s_DiagRenders = 0;

    // Function pointer for eViewPlatInterface::Render (0x005C5930)
    typedef void (__thiscall* MeshRenderFn)(void* eViewPlat, void* model, bMatrix4* mtx, void* lightCtx, uint32_t flags, uint32_t flags2);
    static MeshRenderFn s_OrigMeshRender = (MeshRenderFn)0x005C5930;

    // Original CarDamage::AddDamage (0x637C70) - the authentic part-damage distributor
    typedef void (__thiscall* AddDamageFn)(void* carDamage, bVector3* localPos, float force);
    static AddDamageFn s_OrigCarDamage_AddDamage = (AddDamageFn)0x00637C70;

    // Original CarDamage::InitSectorPositions (0x610E00) - fills the damage grid bounds
    typedef void (__thiscall* InitSectorPositionsFn)(void* carPartDamage, void* eModel);
    static InitSectorPositionsFn s_OrigInitSectorPositions = (InitSectorPositionsFn)0x00610E00;

    // Dedicated per-part wobbled matrix buffers (prevents any cross-part matrix collisions)
    static bMatrix4 s_PartWobbleMatrix[6];

    // Render-time wobble state, consumed by the shared mesh render hook
    extern "C" {
        uintptr_t s_CurrentCollisionBody = 0;
        bMatrix4* s_ActiveWobbleMatrix = nullptr;   // pure pivot wobble (pre-multiply)
        bool s_SkipPartRender = false;              // detached: skip this part's draw
        int s_WobbleUseCarSpace = 0;                // 1 = car-world matrix path (car-space hinge),
                                                    // 0 = per-part matrix path (part-local, zero pivot)
    }

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
    // __thiscall: ecx = CarPartDamage* (located at CarDamage + 0x100)
    void __fastcall Hooked_CarPartDamage_UpdateDamage(void* thisPartDamage, void* edx)
    {
        if (!thisPartDamage)
            return;

        uintptr_t carDamage = (uintptr_t)thisPartDamage - 0x100;
        UpdateDamageStages(carDamage);
    }

    // 2. Hook the CarDamage::AddDamage call site (0x00593C44 in CarCollisionBody::AddDamage).
    // The contact position arrives already transformed into car-local space and the force
    // pre-scaled by 0.013; the original callee distributes damage across the 6 breakable
    // parts through the 4x3x2 sector grid. We only sanitize/amplify the force - the
    // authentic engine chain does the per-part assignment.
    void __fastcall Hooked_AddDamage(void* carDamage, void* edx, bVector3* localPos, float force)
    {
        if (!carDamage)
            return;

        // Ensure global damage flag is enabled
        *(uint8_t*)0x0089E7D2 = 1;

        float amplified = force;
        if (localPos && force > 0.0f)
        {
            amplified = force * 2.5f;

            if (s_LogHits < 24)
            {
                float* dmg = (float*)((uintptr_t)carDamage + 0x120);
                Logger::Log("[DAMAGE] Hit: force=%.3f pos=(%.2f, %.2f, %.2f) | Dmg: M_L=%.2f M_R=%.2f Exh=%.2f Trk=%.2f D_L=%.2f D_R=%.2f",
                    force, localPos->x, localPos->y, localPos->z,
                    dmg[0], dmg[1], dmg[2], dmg[3], dmg[4], dmg[5]);
                s_LogHits++;
            }
        }

        s_OrigCarDamage_AddDamage(carDamage, localPos, amplified);
    }

    // 3. Hook CarDamage::RecalculateBreakableDamage caller at 0x00633F08.
    // Preserve part stages in bits 16..31 across body deformation recalculations.
    typedef uint32_t (__thiscall* ComputeDamageBitfieldFn)(void* thisBody);
    static ComputeDamageBitfieldFn s_OrigComputeDamageBitfield = (ComputeDamageBitfieldFn)0x00610EA0;

    uint32_t __fastcall Hooked_ComputeDamageBitfield(void* thisBody, void* edx)
    {
        uint32_t bodyStages = s_OrigComputeDamageBitfield(thisBody);
        uint32_t currentE0 = *(uint32_t*)((uintptr_t)thisBody + 0xE0);
        return (bodyStages & 0x0000FFFF) | (currentE0 & 0xFFFF0000);
    }

    // 4. Hook the CarDamage::InitSectorPositions call site (0x0063892A in the
    // CarRenderInfo constructor). The original only fills the damage grid bounds when the
    // car sub-model hash is one of the 15 entries at 0x8036FC; for every other car the
    // grid stays invalid and FindDamageSector collapses onto cell 0 (parts 3/5 only).
    // After the original runs, patch a generic full-car grid for cars it could not map.
    void __fastcall Hooked_InitSectorPositions(void* carPartDamage, void* edx, void* eModel)
    {
        s_OrigInitSectorPositions(carPartDamage, eModel);

        if (!carPartDamage)
            return;

        float* gridMin = (float*)((uintptr_t)carPartDamage + 0x50);
        float* gridMax = (float*)((uintptr_t)carPartDamage + 0x60);

        bool invalid = (gridMax[0] - gridMin[0]) <= 0.01f ||
                       (gridMax[1] - gridMin[1]) <= 0.01f ||
                       (gridMax[2] - gridMin[2]) <= 0.01f;

        if (invalid)
        {
            // Generic car-local bounds: x = length (4 cells), y = width (3 cells), z = height (2 cells)
            gridMin[0] = -2.5f; gridMin[1] = -1.3f; gridMin[2] = -0.3f;
            gridMax[0] =  2.5f; gridMax[1] =  1.3f; gridMax[2] =  1.8f;
        }
    }

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
    // around the given hinge pivot point P, keeping the hinge invariant (P * M = P).
    // A zero pivot rotates the part around its own origin (pure rotation, zero translation).
    static void BuildPivotWobbleMatrix(const float* p, const bVector3* angles, bMatrix4* out)
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

        // Invariant hinge pivot: T = P - P * R
        float tx = p[0] - (p[0] * r00 + p[1] * r10 + p[2] * r20);
        float ty = p[1] - (p[0] * r01 + p[1] * r11 + p[2] * r21);
        float tz = p[2] - (p[0] * r02 + p[1] * r12 + p[2] * r22);

        out->m[0][0] = r00; out->m[0][1] = r01; out->m[0][2] = r02; out->m[0][3] = 0.0f;
        out->m[1][0] = r10; out->m[1][1] = r11; out->m[1][2] = r12; out->m[1][3] = 0.0f;
        out->m[2][0] = r20; out->m[2][1] = r21; out->m[2][2] = r22; out->m[2][3] = 0.0f;
        out->m[3][0] = tx;  out->m[3][1] = ty;  out->m[3][2] = tz;  out->m[3][3] = 1.0f;
    }

    // 5. Physical Wobble Angle Calculation for all 6 vehicle damage parts.
    // Matches the PS2 Demo's behaviour (see the demo gameplay video): damaged parts
    // CONTINUOUSLY FLAP around their hinge - doors swing open and closed, the trunk pops
    // up and slams down, mirrors buzz - instead of sitting stuck at a static angle.
    // Every angle is a centred oscillation (sin cycles) scaled by the damage stage.
    static void ComputePartWobbleAngles(int partIndex, float stage, bVector3* outAngles)
    {
        outAngles->x = 0.0f;
        outAngles->y = 0.0f;
        outAngles->z = 0.0f;

        if (stage <= 0.0f) return;

        float t = s_SimTime;
        float scale = g_Config.wobbleScale;
        float s = stage - 1.0f; // 0..2 progressive amplitude factor

        // flapCycle: 0..1..0 slow swing (the part opens and closes repeatedly)
        // rattle:    fast high-frequency vibration on top
        float flapDoor  = std::sin(t * 9.0f) * 0.5f + 0.5f;        // ~1.4 Hz
        float flapTrunk = std::sin(t * 11.0f) * 0.5f + 0.5f;       // ~1.75 Hz

        switch (partIndex)
        {
        case 0: // Left Mirror - buzzes/vibrates in place
        {
            outAngles->x = std::sin(t * 27.0f) * 0.06f * stage * scale;
            outAngles->z = std::sin(t * 33.0f + 0.8f) * 0.05f * stage * scale;
            break;
        }
        case 1: // Right Mirror - buzzes/vibrates in place
        {
            outAngles->x = std::sin(t * 29.0f + 1.1f) * 0.06f * stage * scale;
            outAngles->z = -std::sin(t * 31.0f + 1.7f) * 0.05f * stage * scale;
            break;
        }
        case 2: // Exhaust - sags slightly and vibrates
        {
            outAngles->y = (-(0.03f + 0.025f * s) + std::sin(t * 19.0f) * 0.03f * stage) * scale;
            outAngles->z = std::sin(t * 26.0f + 0.4f) * 0.035f * stage * scale;
            break;
        }
        case 3: // Trunk Lid - pops up and slams down repeatedly
        {
            outAngles->y = (flapTrunk * (0.06f + 0.07f * s) + std::sin(t * 17.0f) * 0.02f * stage) * scale;
            outAngles->x = std::sin(t * 21.0f) * 0.015f * stage * scale;
            break;
        }
        case 4: // Left Door - flaps open and closed around the front hinge
        {
            outAngles->z = -(flapDoor * (0.05f + 0.08f * s) + std::sin(t * 22.0f) * 0.02f * stage) * scale;
            outAngles->x = std::sin(t * 28.0f) * 0.012f * stage * scale;
            break;
        }
        case 5: // Right Door - flaps open and closed around the front hinge
        {
            outAngles->z = (flapDoor * (0.05f + 0.08f * s) + std::sin(t * 23.0f + 0.6f) * 0.02f * stage) * scale;
            outAngles->x = std::sin(t * 27.0f + 0.7f) * 0.012f * stage * scale;
            break;
        }
        default:
            break;
        }
    }

    // 6. Damage evaluation for the generic part render blocks. Returns 0 = intact,
    // 1 = wobbling (s_ActiveWobbleMatrix holds the pure pivot wobble to pre-multiply at
    // the render call), 2 = detached (s_SkipPartRender set; the render hook skips the draw).
    //
    // The pivot space depends on which block is rendering (s_WobbleUseCarSpace, set by the
    // block caves): the car-world-matrix block transforms car-space vertices, so the
    // configured car-space hinge applies; the per-part-matrix block transforms part-local
    // vertices near their own origin, so the pivot MUST be zero (a non-zero pivot there
    // orbits the part around a distant point and it launches off the car).
    extern "C" int EvaluatePartDamage(int partIndex, uintptr_t carDamage)
    {
        s_ActiveWobbleMatrix = nullptr;
        s_SkipPartRender = false;

        if (!carDamage || partIndex < 0 || partIndex > 5)
        {
            if (s_DiagEvals < 20)
            {
                s_DiagEvals++;
                Logger::Log("[DIAG] Eval REJECT: partIndex=%d carDamage=0x%08X", partIndex, (unsigned int)carDamage);
            }
            return 0;
        }

        float* dmgArray = (float*)(carDamage + 0x120);
        float dmg = dmgArray[partIndex];

        if (s_DiagEvals < 30)
        {
            s_DiagEvals++;
            Logger::Log("[DIAG] Eval: part=%d space=%s dmg=%.2f", partIndex, s_WobbleUseCarSpace ? "CAR" : "PART", dmg);
        }

        // Detachment / Break-off threshold: 4.0f
        if (dmg >= 4.0f)
        {
            s_SkipPartRender = true;
            return 2;
        }

        // Wobbling threshold: 0.25f (engine-native IsPartDamaged threshold)
        if (dmg >= 0.25f)
        {
            static const float zeroPivot[3] = { 0.0f, 0.0f, 0.0f };

            float stage = (dmg >= 2.0f) ? 3.0f : ((dmg >= 1.0f) ? 2.0f : 1.0f);
            bVector3 angles;
            ComputePartWobbleAngles(partIndex, stage, &angles);

            // Car-world-matrix path: vertices are car-space -> configured car-space hinge.
            // Per-part-matrix path: vertices are part-local -> rotate in place (zero pivot).
            const float* pivot = s_WobbleUseCarSpace ? g_Config.partPivots[partIndex] : zeroPivot;
            BuildPivotWobbleMatrix(pivot, &angles, &s_PartWobbleMatrix[partIndex]);
            s_ActiveWobbleMatrix = &s_PartWobbleMatrix[partIndex];
            return 1;
        }

        return 0;
    }

    // 7. Naked code caves at the two generic part render block entries.
    // They run the damage evaluation before the block executes; the block then falls
    // through to the shared eMesh::Render call where the wobble matrix is composed with
    // whichever matrix that block passes (car-world at 0x624441, per-part at 0x624404).
    // Detached parts are skipped inside the render hook - no malloc, no leak.
    __attribute__((naked)) static void PartBlock_CarMatrix_Cave() // 0x00624441
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "pushad\n"
            "mov dword ptr _s_WobbleUseCarSpace, 1\n" // this block passes the car-world matrix
            "push dword ptr [esp + 0x180 + 32]\n"   // carDamage = orig [esp+0x180]
            "push dword ptr [esp + 0x64 + 32 + 4]\n"// partIndex = orig [esp+0x64]
            "call _EvaluatePartDamage\n"
            "add esp, 8\n"
            "mov [esp + 0x1C], eax\n"               // store result into saved EAX slot
            "popad\n"
            "push 0x0062446A\n"                     // always take the intact/draw path
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    __attribute__((naked)) static void PartBlock_PerPartMatrix_Cave() // 0x00624404
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "pushad\n"
            "mov dword ptr _s_WobbleUseCarSpace, 0\n" // this block passes a per-part matrix
            "push dword ptr [esp + 0x180 + 32]\n"   // carDamage = orig [esp+0x180]
            "push dword ptr [esp + 0x6C + 32 + 4]\n"// partIndex = orig [esp+0x6C]
            "call _EvaluatePartDamage\n"
            "add esp, 8\n"
            "mov [esp + 0x1C], eax\n"               // store result into saved EAX slot
            "popad\n"
            "mov edx, dword ptr [esp + 0xE8]\n"     // re-execute the replaced original instruction
            "push 0x0062440B\n"                     // resume the original block
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    // 8. Shared mesh render hook at 0x00624499 (both generic part blocks funnel here).
    // Applies the active wobble matrix, or skips the draw entirely for detached parts.
    void __thiscall Hooked_BreakablePartMeshRender(void* eView, void* model, bMatrix4* mtx, void* lightCtx, uint32_t flags, uint32_t flags2)
    {
        if (s_SkipPartRender)
        {
            s_SkipPartRender = false;
            s_ActiveWobbleMatrix = nullptr;
            if (s_DiagRenders < 15)
            {
                s_DiagRenders++;
                Logger::Log("[DIAG] Render: part SKIPPED (detached)");
            }
            return; // detached: draw nothing (callee cleans the stack like the original)
        }

        bMatrix4* actualMtx = mtx;
        if (s_ActiveWobbleMatrix && mtx)
        {
            MultiplyMatrices(&s_PartWobbleMatrix[5], s_ActiveWobbleMatrix, mtx);
            actualMtx = &s_PartWobbleMatrix[5];
        }

        if (s_DiagRenders < 15 && mtx)
        {
            s_DiagRenders++;
            Logger::Log("[DIAG] Render: wobble=%s mtxT=(%.1f, %.1f, %.1f) row0=(%.2f, %.2f, %.2f)",
                s_ActiveWobbleMatrix ? "YES" : "no",
                mtx->m[3][0], mtx->m[3][1], mtx->m[3][2],
                mtx->m[0][0], mtx->m[0][1], mtx->m[0][2]);
        }

        s_ActiveWobbleMatrix = nullptr;
        s_OrigMeshRender(eView, model, actualMtx, lightCtx, flags, flags2);
    }

    // 9. Code caves to capture/zero CarDamage* during CarRenderInfo::Render
    __attribute__((naked)) static void CaptureCurrentCollisionBodyCodeCave()
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "mov [esp + 0x180], eax\n"
            "mov _s_CurrentCollisionBody, eax\n"
            "mov dword ptr _s_ActiveWobbleMatrix, 0\n"
            "mov dword ptr _s_SkipPartRender, 0\n"
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
            "mov dword ptr _s_ActiveWobbleMatrix, 0\n"
            "mov dword ptr _s_SkipPartRender, 0\n"
            "push 0x00621092\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    // 10. Left Mirror and Right Mirror mesh render hooks. The mirror blocks pass their own
    // matrix (space unverified), so the mirrors always vibrate with a zero pivot - they can
    // never detach. Set PivotMirrorL/R in the INI to experiment with a real hinge.
    static void RenderMirrorWithWobble(int partIndex, float damageOffset, void* eViewPlat, void* model, bMatrix4* mtx, void* lightCtx, uint32_t flags, uint32_t flags2)
    {
        bMatrix4* renderMtx = mtx;
        if (s_CurrentCollisionBody && mtx)
        {
            float dmg = *(float*)(s_CurrentCollisionBody + (uintptr_t)damageOffset);
            if (dmg < 4.0f && dmg >= 0.25f)
            {
                float stage = (dmg >= 2.0f) ? 3.0f : ((dmg >= 1.0f) ? 2.0f : 1.0f);
                bVector3 angles;
                ComputePartWobbleAngles(partIndex, stage, &angles);

                static const float zeroPivot[3] = { 0.0f, 0.0f, 0.0f };
                const float* pivot = (g_Config.partPivots[partIndex][0] != 0.0f ||
                                      g_Config.partPivots[partIndex][1] != 0.0f ||
                                      g_Config.partPivots[partIndex][2] != 0.0f)
                                         ? g_Config.partPivots[partIndex]
                                         : zeroPivot;
                bMatrix4 wobbleMtx;
                BuildPivotWobbleMatrix(pivot, &angles, &wobbleMtx);
                MultiplyMatrices(&s_PartWobbleMatrix[partIndex], &wobbleMtx, mtx);
                renderMtx = &s_PartWobbleMatrix[partIndex];
            }
        }
        s_OrigMeshRender(eViewPlat, model, renderMtx, lightCtx, flags, flags2);
    }

    void __thiscall Hooked_RenderLeftMirrorMesh(void* eViewPlat, void* model, bMatrix4* mtx, void* lightCtx, uint32_t flags, uint32_t flags2)
    {
        RenderMirrorWithWobble(0, 0x120, eViewPlat, model, mtx, lightCtx, flags, flags2);
    }

    void __thiscall Hooked_RenderRightMirrorMesh(void* eViewPlat, void* model, bMatrix4* mtx, void* lightCtx, uint32_t flags, uint32_t flags2)
    {
        RenderMirrorWithWobble(1, 0x124, eViewPlat, model, mtx, lightCtx, flags, flags2);
    }

    // 11. Initialize table 0x8A0388 (24 grid cells x 6 part weights) with non-zero weights
    // so every impact location delivers damage. InitCarPartDamageSectors (0x610B00) only
    // populates a handful of authentic cells; zero-weight cells swallow all damage.
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
        //    +0x00 is the enable gate checked by CarPartDamage::IsPartDamaged (all zero in vanilla).
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

        // 6. Bypass the 0.05 force threshold gate in CarDamage::AddDamage (0x00637C7A)
        injector::MakeNOP(0x00637C7A, 6, true);

        // 7. Hook the CarDamage::AddDamage call site (0x00593C44): sanitize + amplify force,
        //    let the authentic engine chain distribute damage across all 6 parts via the grid.
        injector::MakeCALL(0x00593C44, (void*)Hooked_AddDamage, true);

        // 8. Hook CarDamage::InitSectorPositions call site (0x0063892A): provide a generic
        //    damage grid for car models missing from the 15-entry sector layout table.
        injector::MakeCALL(0x0063892A, (void*)Hooked_InitSectorPositions, true);

        // 9. Hook CarPartDamage::UpdateDamage (0x00610CD0) to calculate and pack part stages
        //    into +0xE0 (for window cracking textures)
        injector::MakeJMP(0x00610CD0, (void*)Hooked_CarPartDamage_UpdateDamage, true);

        // 10. Hook RecalculateBreakableDamage caller at 0x00633F08 to preserve part stages in +0xE0
        injector::MakeCALL(0x00633F08, (void*)Hooked_ComputeDamageBitfield, true);

        // 11. Capture and zero CarDamage* during CarRenderInfo::Render
        injector::MakeJMP(0x00621087, (void*)ZeroCurrentCollisionBodyCodeCave, true);
        injector::MakeNOP(0x00621087 + 5, 6, true);

        injector::MakeJMP(0x0062109C, (void*)CaptureCurrentCollisionBodyCodeCave, true);
        injector::MakeNOP(0x0062109C + 5, 2, true);

        // 12. Hook BOTH generic part render blocks:
        //     0x00624404 (per-part matrix path - used while driving)
        //     0x00624441 (car-world matrix path); original head 'mov eax,[esp+0x64]' is 4
        //     bytes, the cave replaces the whole vanilla IsPartDamaged/malloc/skip flow.
        injector::MakeJMP(0x00624404, (void*)PartBlock_PerPartMatrix_Cave, true);
        injector::MakeNOP(0x00624404 + 5, 2, true); // tail of 'mov edx,[esp+0xE8]' (7 bytes)

        injector::MakeJMP(0x00624441, (void*)PartBlock_CarMatrix_Cave, true);
        injector::MakeNOP(0x00624441 + 5, 36, true); // 0x62446A - 0x624446 = 36 bytes

        // 13. Hook the shared mesh render call at 0x00624499: composes the wobble matrix
        //     with whichever matrix the block passes, skips detached parts
        injector::MakeCALL(0x00624499, (void*)Hooked_BreakablePartMeshRender, true);

        // 14. Fix Left Mirror detachment threshold (0x006264F6) to compare against 4.0f (0x00803940)
        injector::WriteMemory<uint32_t>(0x006264F6, 0x00803940, true);

        // 15. Hook Left Mirror mesh render call at 0x006269F0 for dynamic wobbling
        injector::MakeCALL(0x006269F0, (void*)Hooked_RenderLeftMirrorMesh, true);

        // 16. Fix Right Mirror detachment threshold (0x00626A67) to compare against 4.0f (0x00803940)
        injector::WriteMemory<uint32_t>(0x00626A67, 0x00803940, true);

        // 17. Hook Right Mirror mesh render call at 0x00626FED for dynamic wobbling
        injector::MakeCALL(0x00626FED, (void*)Hooked_RenderRightMirrorMesh, true);

        // 18. Initialize collision weights table
        InitDamageWeightsTable();

        Logger::Log("[+] VehicleDamageRestorations: 6 parts enabled, authentic grid damage delivery, "
                    "render-time wobble composition on both part paths (car-matrix + per-part-matrix), "
                    "detachment at 4.0f, wobble scale %.2f", g_Config.wobbleScale);
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

        uintptr_t carDamage = *(uintptr_t*)((uintptr_t)car + 0x958);
        if (!carDamage)
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

        float* damageArray = (float*)(carDamage + 0x120);
        for (int i = 0; i < 6; ++i)
        {
            damageArray[i] = dmg;
        }

        UpdateDamageStages(carDamage);
        Logger::Log("[DAMAGE] F7 Hotkey: Set player vehicle damage to %.2f -> %s (CarDamage* 0x%08X), parts now: %.2f %.2f %.2f %.2f %.2f %.2f",
            dmg, stageName, (unsigned int)carDamage,
            damageArray[0], damageArray[1], damageArray[2], damageArray[3], damageArray[4], damageArray[5]);
    }
}
