#pragma once

#include <cstdint>
#include <cstddef>

namespace FEngRestorations
{
    // External reference to embedded HUD_CarShow.fng binary data
    extern const size_t g_HUD_CarShow_FNG_Size;
    extern const uint8_t g_HUD_CarShow_FNG[];

    // Installs HUD_CarShow.fng from embedded ASI binary into FEng runtime
    // Only called when Burnout race mode actually requires the package
    bool InstallEmbeddedHUDCarShow();

    // Directly installs raw FNG package data from memory
    bool LoadFNGPackageFromMemory(const void* data, size_t size);
}
