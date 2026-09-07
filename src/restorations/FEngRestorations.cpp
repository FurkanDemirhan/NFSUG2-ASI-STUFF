#include "FEngRestorations.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <unordered_set>
#include "includes/injector/injector.hpp"
#include "includes/injector/hooking.hpp"
#include "Logger.h"

namespace FEngRestorations
{
    // Function pointers in SPEED2.EXE
    // 0x005425A0: FEngInstallPackage(void* pChunk)
    typedef int (__cdecl* FEngInstallPackageFn)(void* pChunk);
    static FEngInstallPackageFn FEngInstallPackage = (FEngInstallPackageFn)0x005425A0;

    // Trampoline for original FEngFindPackage at 0x0052CEF0
    __attribute__((naked)) static void* __cdecl Original_FEngFindPackage(const char* pkg_name)
    {
        asm volatile (
            ".intel_syntax noprefix\n"
            "mov eax, dword ptr ds:[0x8384D0]\n"
            "jmp 0x0052CEF5\n"
            ".att_syntax prefix\n"
        );
    }

    bool LoadFNGPackageFromMemory(const void* data, size_t size)
    {
        if (!data || size < 16)
        {
            return false;
        }

        // Wrap raw FNG stream in an uncompressed 0x00030203 chunk header
        // Header: uint32_t chunk_id (0x00030203), uint32_t chunk_size (size)
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

    bool LoadFNGPackageFromFile(const char* filepath)
    {
        FILE* fp = fopen(filepath, "rb");
        if (!fp)
        {
            return false;
        }

        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        if (sz <= 0)
        {
            fclose(fp);
            return false;
        }

        std::vector<uint8_t> buffer(sz);
        size_t read_bytes = fread(buffer.data(), 1, sz, fp);
        fclose(fp);

        if (read_bytes != (size_t)sz)
        {
            return false;
        }

        Logger::Log("[FEng] Loading loose package from file: %s (%ld bytes)", filepath, sz);
        return LoadFNGPackageFromMemory(buffer.data(), buffer.size());
    }

    static std::unordered_set<std::string> s_AttemptedPackages;

    static void* __cdecl Hooked_FEngFindPackage(const char* pkg_name)
    {
        void* pkg = Original_FEngFindPackage(pkg_name);
        if (pkg != nullptr)
        {
            return pkg;
        }

        if (!pkg_name || !pkg_name[0])
        {
            return nullptr;
        }

        std::string nameStr(pkg_name);
        if (s_AttemptedPackages.find(nameStr) != s_AttemptedPackages.end())
        {
            return nullptr;
        }
        s_AttemptedPackages.insert(nameStr);

        // Search paths:
        // 1. FRONTEND/<pkg_name>
        // 2. GLOBAL/<pkg_name>
        // 3. FRONTEND/<pkg_name>.fng (if not ending in .fng)
        // 4. GLOBAL/<pkg_name>.fng (if not ending in .fng)
        std::vector<std::string> candidates;
        candidates.push_back(std::string("FRONTEND/") + pkg_name);
        candidates.push_back(std::string("GLOBAL/") + pkg_name);
        if (nameStr.find(".fng") == std::string::npos && nameStr.find(".FNG") == std::string::npos)
        {
            candidates.push_back(std::string("FRONTEND/") + pkg_name + ".fng");
            candidates.push_back(std::string("GLOBAL/") + pkg_name + ".fng");
        }

        for (const auto& path : candidates)
        {
            DWORD attrib = GetFileAttributesA(path.c_str());
            if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY))
            {
                if (LoadFNGPackageFromFile(path.c_str()))
                {
                    return Original_FEngFindPackage(pkg_name);
                }
            }
        }

        return nullptr;
    }

    void Init()
    {
        Logger::Log("[FEng] Installing loose FNG package loader...");
        injector::MakeJMP(0x0052CEF0, (void*)Hooked_FEngFindPackage, true);
        Logger::Log("[FEng] Loose FNG package loader installed at 0x0052CEF0.");
    }
}
