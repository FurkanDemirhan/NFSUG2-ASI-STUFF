#include "Logger.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <windows.h>

static FILE* s_LogFile = nullptr;

namespace Logger
{
    void Init(const char* logFilename)
    {
        if (s_LogFile)
        {
            fclose(s_LogFile);
            s_LogFile = nullptr;
        }

        // Determine exact path next to the game executable
        char fullPath[MAX_PATH] = {0};
        GetModuleFileNameA(NULL, fullPath, MAX_PATH);
        char* lastSlash = strrchr(fullPath, '\\');
        if (!lastSlash)
        {
            lastSlash = strrchr(fullPath, '/');
        }

        if (lastSlash)
        {
            *(lastSlash + 1) = '\0';
            strcat(fullPath, logFilename);
        }
        else
        {
            strncpy(fullPath, logFilename, MAX_PATH - 1);
        }

        s_LogFile = fopen(fullPath, "w");
        if (s_LogFile)
        {
            Log("==================================================");
            Log("  NFS Underground 2 Code Restoration Plugin v1.0  ");
            Log("==================================================");
        }
    }

    void Log(const char* fmt, ...)
    {
        if (!s_LogFile)
            return;

        va_list args;
        va_start(args, fmt);
        vfprintf(s_LogFile, fmt, args);
        fprintf(s_LogFile, "\n");
        fflush(s_LogFile);
        va_end(args);
    }
}
