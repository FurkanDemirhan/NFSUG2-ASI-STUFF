#include "HealthBarRenderer.h"
#include "VehicleHealthManager.h"
#include "Config.h"
#include "Logger.h"
#include "../includes/injector/injector.hpp"
#include "../includes/injector/hooking.hpp"
#include <algorithm>
#include <cmath>

namespace HealthBarRenderer
{
    struct D3DVertex
    {
        float x, y, z, rhw;
        DWORD color;
    };
    #define D3DFVF_HEALTHVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)



    static void DrawSolidRect(IDirect3DDevice9* dev, float x, float y, float w, float h, DWORD color)
    {
        if (w <= 0.0f || h <= 0.0f) return;
        D3DVertex v[4] = {
            { x,     y,     0.0f, 1.0f, color },
            { x + w, y,     0.0f, 1.0f, color },
            { x,     y + h, 0.0f, 1.0f, color },
            { x + w, y + h, 0.0f, 1.0f, color }
        };
        dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(D3DVertex));
    }

    static void DrawTriangle(IDirect3DDevice9* dev, float x1, float y1, float x2, float y2, float x3, float y3, DWORD color)
    {
        D3DVertex v[3] = {
            { x1, y1, 0.0f, 1.0f, color },
            { x2, y2, 0.0f, 1.0f, color },
            { x3, y3, 0.0f, 1.0f, color }
        };
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, v, sizeof(D3DVertex));
    }

    static DWORD LerpColor(DWORD c1, DWORD c2, float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        int a = (int)(((c1 >> 24) & 0xFF) * (1.0f - t) + ((c2 >> 24) & 0xFF) * t);
        int r = (int)(((c1 >> 16) & 0xFF) * (1.0f - t) + ((c2 >> 16) & 0xFF) * t);
        int g = (int)(((c1 >> 8) & 0xFF) * (1.0f - t) + ((c2 >> 8) & 0xFF) * t);
        int b = (int)(((c1) & 0xFF) * (1.0f - t) + ((c2) & 0xFF) * t);
        return ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
    }

    static DWORD GetHealthBarColor(float pct)
    {
        if (pct > 0.5f)
        {
            float t = (pct - 0.5f) * 2.0f;
            return LerpColor(0xFFFFC107, 0xFF00E676, t); // Amber -> Green
        }
        else
        {
            float t = pct * 2.0f;
            return LerpColor(0xFFFF1744, 0xFFFFC107, t); // Red -> Amber
        }
    }

    typedef void* (__cdecl *GetViewFn)(int index);
    static auto s_GetView = (GetViewFn)0x0048B1E0;

    typedef void (__thiscall *ViewWorldToScreenFn)(void* pView, bVector3* outScreen, const bVector3* inWorld);
    static auto s_ViewWorldToScreen = (ViewWorldToScreenFn)0x005BC4A0;

    static bool ProjectWorldToScreen(const bVector3& worldPos, const D3DVIEWPORT9& vp, float& outScreenX, float& outScreenY, float& outNdcZ)
    {
        // View 1 (0x00832E50) is the active 3D world gameplay view; View 0 (0x00832DE0) is for 2D UI
        void* pView = s_GetView ? s_GetView(1) : (void*)0x00832E50;
        if (!pView || !*(uintptr_t*)pView)
        {
            static bool s_LoggedNoView = false;
            if (!s_LoggedNoView)
            {
                s_LoggedNoView = true;
                VehicleHealthLogger::Log("[Project Error] pView or *(uintptr_t*)pView is null! (pView=%p)", pView);
            }
            return false;
        }

        int screenW = *(int*)0x00870980;
        int screenH = *(int*)0x00870984;
        if (screenW <= 0) screenW = (vp.Width > 0) ? (int)vp.Width : 640;
        if (screenH <= 0) screenH = (vp.Height > 0) ? (int)vp.Height : 480;

        // 1. Primary projection: eView::WorldToScreen (0x005BC4A0) on View 1
        bVector3 screenPos = { 0.0f, 0.0f, 0.0f };
        s_ViewWorldToScreen(pView, &screenPos, &worldPos);

        // Debug logging for first 30 projection attempts
        static int s_DebugProjCount = 0;
        if (s_DebugProjCount < 30)
        {
            s_DebugProjCount++;
            VehicleHealthLogger::Log("[Proj #%d] World(%.1f, %.1f, %.1f) -> Screen(%.1f, %.1f, Zndc=%.4f) Res(%d x %d) VP(%u x %u)",
                                     s_DebugProjCount, worldPos.x, worldPos.y, worldPos.z,
                                     screenPos.x, screenPos.y, screenPos.z, screenW, screenH, vp.Width, vp.Height);
        }

        // Direct3D Zndc clipping: points in front of camera are in (0.0f, 1.0f]
        if (screenPos.z > 0.0f && screenPos.z <= 1.05f)
        {
            outScreenX = screenPos.x * ((float)vp.Width / (float)screenW);
            outScreenY = screenPos.y * ((float)vp.Height / (float)screenH);
            outNdcZ    = screenPos.z;

            // Frustum culling check with safety margin
            if (outScreenX >= -150.0f && outScreenX <= (float)vp.Width + 150.0f &&
                outScreenY >= -150.0f && outScreenY <= (float)vp.Height + 150.0f)
            {
                return true;
            }
        }

        // 2. Camera matrix projection fallback (using Camera struct at pView + 0x40, identical to CarRenderInfo::Neon)
        uintptr_t camera = *(uintptr_t*)((uintptr_t)pView + 0x40);
        if (camera && camera > 0x00400000)
        {
            float camX = *(float*)(camera + 0x40);
            float camY = *(float*)(camera + 0x44);
            float camZ = *(float*)(camera + 0x48);

            float rX = *(float*)(camera + 0x00), rY = *(float*)(camera + 0x10), rZ = *(float*)(camera + 0x20);
            float uX = *(float*)(camera + 0x04), uY = *(float*)(camera + 0x14), uZ = *(float*)(camera + 0x24);
            float fX = *(float*)(camera + 0x08), fY = *(float*)(camera + 0x18), fZ = *(float*)(camera + 0x28);

            float dx = worldPos.x - camX;
            float dy = worldPos.y - camY;
            float dz = worldPos.z - camZ;

            float camSpaceX = dx * rX + dy * rY + dz * rZ;
            float camSpaceY = dx * uX + dy * uY + dz * uZ;
            float camSpaceZ = dx * fX + dy * fY + dz * fZ;

            if (camSpaceZ > 0.5f)
            {
                uint16_t fovVal = *(uint16_t*)(camera + 0xC4);
                float fovRad = (float)fovVal * (3.14159265f / 32768.0f);
                if (fovRad < 0.1f || fovRad > 3.0f) fovRad = 1.05f;
                float tanHalfFov = std::tan(fovRad * 0.5f);
                float aspect = (float)vp.Width / (float)vp.Height;

                float ndcX = camSpaceX / (camSpaceZ * tanHalfFov * aspect);
                float ndcY = camSpaceY / (camSpaceZ * tanHalfFov);

                outScreenX = (float)vp.X + (ndcX + 1.0f) * 0.5f * (float)vp.Width;
                outScreenY = (float)vp.Y + (1.0f - ndcY) * 0.5f * (float)vp.Height;
                outNdcZ    = camSpaceZ;

                if (outScreenX >= -150.0f && outScreenX <= (float)vp.Width + 150.0f &&
                    outScreenY >= -150.0f && outScreenY <= (float)vp.Height + 150.0f)
                {
                    return true;
                }
            }
        }

        return false;
    }

    void Render(IDirect3DDevice9* pDevice)
    {
        if (!pDevice) return;

        D3DVIEWPORT9 vp;
        if (FAILED(pDevice->GetViewport(&vp)) || vp.Width == 0 || vp.Height == 0)
            return;

        // Hide health bars if the post-race / Event Results screen is active during races
        if (!VehicleHealthManager::IsInFreeRoam())
        {
            uintptr_t postRaceScreenMgr = *(uintptr_t*)0x008363DC;
            if (postRaceScreenMgr != 0)
                return;
        }

        auto vehicles = VehicleHealthManager::GetActiveVehiclesForRender();
        if (vehicles.empty()) return;

        static int s_RenderLogCounter = 0;
        if (s_RenderLogCounter < 5)
        {
            s_RenderLogCounter++;
            VehicleHealthLogger::Log("[Render] Active gameplay frame: rendering health bars for %zu vehicles (FreeRoam=%d).",
                                     vehicles.size(), VehicleHealthManager::IsInFreeRoam() ? 1 : 0);
        }

        // Save Direct3D render states to avoid per-frame COM allocations and state leaks
        DWORD oldLighting, oldZEnable, oldZWrite, oldAlphaBlend, oldSrcBlend, oldDestBlend, oldCull, oldFog, oldScissor;
        pDevice->GetRenderState(D3DRS_LIGHTING, &oldLighting);
        pDevice->GetRenderState(D3DRS_ZENABLE, &oldZEnable);
        pDevice->GetRenderState(D3DRS_ZWRITEENABLE, &oldZWrite);
        pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &oldAlphaBlend);
        pDevice->GetRenderState(D3DRS_SRCBLEND, &oldSrcBlend);
        pDevice->GetRenderState(D3DRS_DESTBLEND, &oldDestBlend);
        pDevice->GetRenderState(D3DRS_CULLMODE, &oldCull);
        pDevice->GetRenderState(D3DRS_FOGENABLE, &oldFog);
        pDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &oldScissor);

        DWORD oldColorOp, oldColorArg1, oldAlphaOp, oldAlphaArg1;
        pDevice->GetTextureStageState(0, D3DTSS_COLOROP, &oldColorOp);
        pDevice->GetTextureStageState(0, D3DTSS_COLORARG1, &oldColorArg1);
        pDevice->GetTextureStageState(0, D3DTSS_ALPHAOP, &oldAlphaOp);
        pDevice->GetTextureStageState(0, D3DTSS_ALPHAARG1, &oldAlphaArg1);

        IDirect3DBaseTexture9* oldTexture = nullptr;
        pDevice->GetTexture(0, &oldTexture);
        DWORD oldFVF;
        pDevice->GetFVF(&oldFVF);
        IDirect3DVertexShader9* oldVS = nullptr;
        pDevice->GetVertexShader(&oldVS);
        IDirect3DPixelShader9* oldPS = nullptr;
        pDevice->GetPixelShader(&oldPS);

        // Configure 2D Direct3D render pipeline
        pDevice->SetPixelShader(nullptr);
        pDevice->SetVertexShader(nullptr);
        pDevice->SetFVF(D3DFVF_HEALTHVERTEX);
        pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        pDevice->SetTexture(0, nullptr);
        pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

        for (const auto& entry : vehicles)
        {
            float maxH = std::max(1.0f, entry.health.maxHealth);
            float hpPct = std::clamp(entry.health.currentHealth / maxH, 0.0f, 1.0f);
            float lagPct = std::clamp(entry.health.lagHealth / maxH, 0.0f, 1.0f);

            if (entry.health.isPlayer)
            {
                // Top-center HUD health bar for Player
                float barW = 200.0f;
                float barH = 12.0f;
                float x0 = (float)vp.Width * 0.5f - barW * 0.5f;
                float y0 = 36.0f;

                // 1. Drop shadow
                DrawSolidRect(pDevice, x0 - 3.0f, y0 - 3.0f, barW + 6.0f, barH + 6.0f, 0x99000000);

                // 2. Bezel frame
                DrawSolidRect(pDevice, x0 - 1.5f, y0 - 1.5f, barW + 3.0f, barH + 3.0f, 0xFF37474F);

                // 3. Dark background groove
                DrawSolidRect(pDevice, x0, y0, barW, barH, 0xEE121212);

                // 4. Trailing damage lag bar
                if (lagPct > hpPct)
                {
                    float lagW = barW * lagPct;
                    DrawSolidRect(pDevice, x0, y0, lagW, barH, 0xFFFF6D00); // Vibrant orange lag
                }

                // 5. Foreground health fill bar with gradient
                if (hpPct > 0.0f)
                {
                    float fillW = barW * hpPct;
                    DWORD hpColor = GetHealthBarColor(hpPct);
                    DrawSolidRect(pDevice, x0, y0, fillW, barH, hpColor);

                    // Top glossy sheen highlight
                    DrawSolidRect(pDevice, x0, y0, fillW, barH * 0.4f, 0x33FFFFFF);
                }

                // 6. Tick dividers
                for (float div = 0.25f; div <= 0.75f; div += 0.25f)
                {
                    DrawSolidRect(pDevice, x0 + barW * div, y0, 1.0f, barH, 0x66000000);
                }

                // 7. Player "YOU" pip badge
                DrawSolidRect(pDevice, (float)vp.Width * 0.5f - 18.0f, y0 - 5.0f, 36.0f, 3.0f, 0xFF00E5FF);

                continue;
            }

            // For all OTHER vehicles (AI opponents & traffic):
            // Project their 3D world position to floating coordinates above each car
            float screenX = 0.0f, screenY = 0.0f, ndcZ = 0.0f;
            bool projected = ProjectWorldToScreen(entry.worldRoofPos, vp, screenX, screenY, ndcZ);
            if (!projected)
                continue;

            // Compute true Euclidean distance in meters from player car for perspective scaling
            float depth = 28.0f;
            uintptr_t playerCar = VehicleHealthManager::GetPlayerCar();
            if (playerCar)
            {
                float px = *(float*)(playerCar + 0x60);
                float py = *(float*)(playerCar + 0x64);
                float pz = *(float*)(playerCar + 0x68);
                float dx = entry.worldRoofPos.x - px;
                float dy = entry.worldRoofPos.y - py;
                float dz = entry.worldRoofPos.z - pz;
                depth = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
            if (depth < 1.0f) depth = 1.0f;

            // Don't render bars for cars further than 140 meters away
            if (depth > 140.0f)
                continue;

            // Distance scaling for clean perspective sizing
            // Reference at 28 meters: scale = 1.0 (base 100x10 px)
            float distScale = std::clamp(28.0f / depth, 0.50f, 1.35f);
            float barW = (float)g_HealthConfig.healthBarWidth * distScale;
            float barH = (float)g_HealthConfig.healthBarHeight * distScale;

            // Ensure the bar is never tiny even at a distance
            if (barW < 52.0f) barW = 52.0f;
            if (barH < 6.0f)  barH = 6.0f;

            float x0 = screenX - barW * 0.5f;
            float y0 = screenY - barH * 0.5f;

            // 1. Drop shadow
            DrawSolidRect(pDevice, x0 - 2.0f, y0 - 2.0f, barW + 4.0f, barH + 4.0f, 0x88000000);

            // 2. Bezel frame
            DWORD frameColor = entry.health.isOpponent ? 0xFFFF5722 : 0xFF333333; // Orange/Red accent for Opponent
            DrawSolidRect(pDevice, x0 - 1.0f, y0 - 1.0f, barW + 2.0f, barH + 2.0f, frameColor);

            // 3. Dark background groove
            DrawSolidRect(pDevice, x0, y0, barW, barH, 0xDD141414);

            // 4. Trailing damage lag bar
            if (lagPct > hpPct)
            {
                float lagW = barW * lagPct;
                DrawSolidRect(pDevice, x0, y0, lagW, barH, 0xFFE65100);
            }

            // 5. Active foreground health fill bar
            if (hpPct > 0.0f)
            {
                float fillW = barW * hpPct;
                DWORD hpColor = GetHealthBarColor(hpPct);
                DrawSolidRect(pDevice, x0, y0, fillW, barH, hpColor);

                // Top glossy sheen
                float sheenH = std::max(1.0f, barH * 0.4f);
                DrawSolidRect(pDevice, x0, y0, fillW, sheenH, 0x33FFFFFF);
            }

            // 6. Subtle tick dividers at 25%, 50%, 75%
            for (float div = 0.25f; div <= 0.75f; div += 0.25f)
            {
                DrawSolidRect(pDevice, x0 + barW * div, y0, 1.0f, barH, 0x55000000);
            }

            // 7. Small downward-pointing chevron / pip anchoring to roof
            float arrowW = 8.0f * distScale;
            float arrowH = 5.0f * distScale;
            float ax = screenX;
            float ay = y0 + barH + 2.0f;
            DrawTriangle(pDevice,
                         ax - arrowW * 0.5f, ay,
                         ax + arrowW * 0.5f, ay,
                         ax, ay + arrowH,
                         frameColor);
        }

        // Restore render states
        pDevice->SetRenderState(D3DRS_LIGHTING, oldLighting);
        pDevice->SetRenderState(D3DRS_ZENABLE, oldZEnable);
        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, oldZWrite);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, oldAlphaBlend);
        pDevice->SetRenderState(D3DRS_SRCBLEND, oldSrcBlend);
        pDevice->SetRenderState(D3DRS_DESTBLEND, oldDestBlend);
        pDevice->SetRenderState(D3DRS_CULLMODE, oldCull);
        pDevice->SetRenderState(D3DRS_FOGENABLE, oldFog);
        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, oldScissor);
        pDevice->SetTextureStageState(0, D3DTSS_COLOROP, oldColorOp);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, oldColorArg1);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, oldAlphaOp);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, oldAlphaArg1);
        pDevice->SetTexture(0, oldTexture);
        if (oldTexture) oldTexture->Release();
        pDevice->SetFVF(oldFVF);
        pDevice->SetVertexShader(oldVS);
        if (oldVS) oldVS->Release();
        pDevice->SetPixelShader(oldPS);
        if (oldPS) oldPS->Release();
    }

    static bool s_HookInstalled = false;
    typedef HRESULT (WINAPI* EndSceneFn)(IDirect3DDevice9* pDevice);
    static EndSceneFn s_OrigEndScene = nullptr;

    static HRESULT WINAPI Hooked_EndScene(IDirect3DDevice9* pDevice)
    {
        static bool s_InEndScene = false;
        if (!s_InEndScene && pDevice)
        {
            s_InEndScene = true;

            static int s_RenderTicks = 0;
            if (s_RenderTicks++ < 15)
            {
                VehicleHealthLogger::Log("[+] Hooked_EndScene TICK #%d (Flow=%d)", s_RenderTicks, *(uint32_t*)0x008654A4);
            }

            // Render health bars during active gameplay (Update() is handled by main loop hook)
            uint32_t gameFlow = *(uint32_t*)0x008654A4;
            if (gameFlow == 6 && g_HealthConfig.displayHealthBars)
            {
                Render(pDevice);
            }

            s_InEndScene = false;
        }

        if (s_OrigEndScene)
        {
            return s_OrigEndScene(pDevice);
        }
        return D3D_OK;
    }

    static CRITICAL_SECTION s_HookCs;
    static bool s_CsInit = false;

    static bool InstallHookInternal(const char* caller)
    {
        if (s_HookInstalled)
            return true;

        if (!s_CsInit)
        {
            InitializeCriticalSection(&s_HookCs);
            s_CsInit = true;
        }

        EnterCriticalSection(&s_HookCs);
        if (s_HookInstalled)
        {
            LeaveCriticalSection(&s_HookCs);
            return true;
        }

        uintptr_t pDevice = *(uintptr_t*)0x00870974;
        if (pDevice && pDevice >= 0x00010000)
        {
            uintptr_t* vtable = *(uintptr_t**)pDevice;
            if (vtable && (uintptr_t)vtable >= 0x00010000)
            {
                if (vtable[42] != (uintptr_t)Hooked_EndScene)
                {
                    s_OrigEndScene = (EndSceneFn)vtable[42];
                    DWORD oldProt;
                    VirtualProtect(&vtable[42], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);
                    vtable[42] = (uintptr_t)Hooked_EndScene;
                    VirtualProtect(&vtable[42], sizeof(void*), oldProt, &oldProt);
                    s_HookInstalled = true;
                    VehicleHealthLogger::Log("[+] Direct3D 9 EndScene vtable[42] hooked successfully via %s (pDevice=%p, vtable=%p, Orig: 0x%08lX)",
                                            caller, (void*)pDevice, (void*)vtable, (unsigned long)s_OrigEndScene);
                    LeaveCriticalSection(&s_HookCs);
                    return true;
                }
            }
        }

        LeaveCriticalSection(&s_HookCs);
        return false;
    }

    void CheckInstallHook()
    {
        if (!s_HookInstalled)
        {
            InstallHookInternal("CheckInstallHook");
        }
    }

    static DWORD WINAPI D3DHookThread(LPVOID)
    {
        VehicleHealthLogger::Log("[+] D3DHookThread started. Awaiting Direct3D 9 device creation...");
        while (!s_HookInstalled)
        {
            if (InstallHookInternal("D3DHookThread"))
                return 0;
            Sleep(50);
        }
        return 0;
    }

    void Init()
    {
        // Dispatch background installer thread to hook vtable[42] as soon as the D3D device exists
        CreateThread(NULL, 0, D3DHookThread, NULL, 0, NULL);
        VehicleHealthLogger::Log("[+] HealthBarRenderer initialized (D3DHookThread dispatched).");
    }
}
