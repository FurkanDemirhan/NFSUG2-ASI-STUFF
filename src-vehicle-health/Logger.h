#pragma once
#include <windows.h>
#include <cstdint>

namespace VehicleHealthLogger
{
    void Init(const char* logFilename);
    void Log(const char* fmt, ...);
    void Close();
}
