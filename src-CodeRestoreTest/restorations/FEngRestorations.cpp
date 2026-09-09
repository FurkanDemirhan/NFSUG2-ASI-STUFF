#include "FEngRestorations.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Logger.h"

namespace FEngRestorations
{
    // True FEngInstallPackage in SPEED2.EXE v1.2 NTSC at 0x0051BD30
    // Takes pointer to chunk (0x00030203 chunk_id, uint32_t size, raw FNG data)
    typedef int (__cdecl* FEngInstallPackageFn)(void* pChunk);
    static FEngInstallPackageFn FEngInstallPackage = (FEngInstallPackageFn)0x0051BD30;

    // FEngFindPackage at 0x0052CEF0
    typedef void* (__cdecl* FEngFindPackageFn)(const char* pkg_name);
    static FEngFindPackageFn FEngFindPackage = (FEngFindPackageFn)0x0052CEF0;

    bool LoadFNGPackageFromMemory(const void* data, size_t size)
    {
        if (!data || size < 16)
        {
            return false;
        }

        // FEng retains pointers into the chunk buffer for package headers and names,
        // so chunk memory must remain allocated and valid for the lifetime of the session.
        size_t total_size = size + 8;
        uint8_t* pChunk = (uint8_t*)malloc(total_size);
        if (!pChunk)
        {
            return false;
        }

        *(uint32_t*)(pChunk + 0) = 0x00030203; // FEngFiles uncompressed chunk ID
        *(uint32_t*)(pChunk + 4) = (uint32_t)size;
        memcpy(pChunk + 8, data, size);

        int result = FEngInstallPackage(pChunk);
        Logger::Log("[FEng] Installed FNG package from memory (raw size=%u, result=%d)", (unsigned int)size, result);
        return result != 0;
    }

    bool InstallEmbeddedHUDCarShow()
    {
        static bool s_Installed = false;
        if (s_Installed)
        {
            return true;
        }

        // Check if package is already registered in FEng
        if (FEngFindPackage("HUD_CarShow.fng") != nullptr)
        {
            s_Installed = true;
            return true;
        }

        // Allocate persistent static chunk buffer inside .bss to avoid dynamic allocations
        static uint8_t s_HUDCarShowChunk[8 + 65536];
        if (g_HUD_CarShow_FNG_Size > 65536)
        {
            Logger::Log("[FEng] Error: embedded HUD_CarShow size mismatch (%u > 65536)", (unsigned int)g_HUD_CarShow_FNG_Size);
            return false;
        }

        *(uint32_t*)(s_HUDCarShowChunk + 0) = 0x00030203; // FEngFiles uncompressed chunk ID
        *(uint32_t*)(s_HUDCarShowChunk + 4) = (uint32_t)g_HUD_CarShow_FNG_Size;
        memcpy(s_HUDCarShowChunk + 8, g_HUD_CarShow_FNG, g_HUD_CarShow_FNG_Size);

        int result = FEngInstallPackage(s_HUDCarShowChunk);
        Logger::Log("[FEng] Installed embedded HUD_CarShow.fng into FEng (result=%d, size=%u)",
            result, (unsigned int)g_HUD_CarShow_FNG_Size);

        s_Installed = (result != 0);
        return s_Installed;
    }
}
