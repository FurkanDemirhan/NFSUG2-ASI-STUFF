#pragma once
#include <windows.h>
#include <d3d9.h>

namespace HealthBarRenderer
{
    void Init();
    void CheckInstallHook();
    void Render(IDirect3DDevice9* pDevice);
}
