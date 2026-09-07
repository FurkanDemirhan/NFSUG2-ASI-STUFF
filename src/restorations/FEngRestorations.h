#pragma once

#include <cstdint>
#include <cstddef>

namespace FEngRestorations
{
    void Init();

    // Loads a loose .fng file from disk and installs it into the FEng runtime
    bool LoadFNGPackageFromFile(const char* filepath);

    // Directly installs raw FNG package data from memory
    bool LoadFNGPackageFromMemory(const void* data, size_t size);
}
