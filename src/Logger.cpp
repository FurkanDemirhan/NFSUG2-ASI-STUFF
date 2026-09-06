#include "Logger.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <windows.h>

static FILE* s_LogFile = nullptr;

static LONG WINAPI VectoredCrashHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION || 
        code == EXCEPTION_ILLEGAL_INSTRUCTION || 
        code == EXCEPTION_DATATYPE_MISALIGNMENT ||
        code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED ||
        code == EXCEPTION_STACK_OVERFLOW ||
        code == 0xC0000005)
    {
        uintptr_t eip = pExceptionInfo->ContextRecord->Eip;
        uintptr_t eax = pExceptionInfo->ContextRecord->Eax;
        uintptr_t ebx = pExceptionInfo->ContextRecord->Ebx;
        uintptr_t ecx = pExceptionInfo->ContextRecord->Ecx;
        uintptr_t edx = pExceptionInfo->ContextRecord->Edx;
        uintptr_t esi = pExceptionInfo->ContextRecord->Esi;
        uintptr_t edi = pExceptionInfo->ContextRecord->Edi;
        uintptr_t ebp = pExceptionInfo->ContextRecord->Ebp;
        uintptr_t esp = pExceptionInfo->ContextRecord->Esp;

        Logger::Log("==================================================");
        Logger::Log("[CRASH] FATAL EXCEPTION 0x%08lX at address 0x%08lX", (unsigned long)code, (unsigned long)eip);
        Logger::Log("[CRASH] Registers:");
        Logger::Log("[CRASH]   EAX=0x%08lX EBX=0x%08lX ECX=0x%08lX EDX=0x%08lX", 
                    (unsigned long)eax, (unsigned long)ebx, (unsigned long)ecx, (unsigned long)edx);
        Logger::Log("[CRASH]   ESI=0x%08lX EDI=0x%08lX EBP=0x%08lX ESP=0x%08lX", 
                    (unsigned long)esi, (unsigned long)edi, (unsigned long)ebp, (unsigned long)esp);

        uintptr_t* stack = (uintptr_t*)esp;
        if (!IsBadReadPtr(stack, 64))
        {
            Logger::Log("[CRASH] Stack dump (ESP):");
            for (int i = 0; i < 16; i++)
            {
                Logger::Log("[CRASH]   [ESP+0x%02X] = 0x%08lX", i * 4, (unsigned long)stack[i]);
            }
        }
        Logger::Log("==================================================");
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

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

        // Install crash handler to catch and diagnose any unhandled exceptions
        AddVectoredExceptionHandler(1, VectoredCrashHandler);
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
