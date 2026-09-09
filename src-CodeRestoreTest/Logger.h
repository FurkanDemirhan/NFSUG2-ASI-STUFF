#pragma once

namespace Logger
{
    void Init(const char* logFilename);
    void Log(const char* fmt, ...);
}
