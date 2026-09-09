#include "Logger.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <windows.h>

static FILE* s_LogFile = nullptr;

static void LogAddressInfo(const char* prefix, uintptr_t addr)
{
    HMODULE hMod = NULL;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &hMod) && hMod)
    {
        char modPath[MAX_PATH] = {0};
        GetModuleFileNameA(hMod, modPath, MAX_PATH);
        char* modName = strrchr(modPath, '\\');
        if (!modName) modName = strrchr(modPath, '/');
        modName = modName ? modName + 1 : modPath;
        uintptr_t offset = addr - (uintptr_t)hMod;
        VehicleHealthLogger::Log("%s0x%08lX (%s+0x%lX, base=0x%08lX)", prefix, (unsigned long)addr, modName, (unsigned long)offset, (unsigned long)hMod);
    }
    else
    {
        VehicleHealthLogger::Log("%s0x%08lX", prefix, (unsigned long)addr);
    }
}

static LONG WINAPI VehicleHealth_VectoredCrashHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    if (!pExceptionInfo || !pExceptionInfo->ExceptionRecord || !pExceptionInfo->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;

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

        VehicleHealthLogger::Log("==================================================");
        VehicleHealthLogger::Log("[CRASH] FATAL EXCEPTION 0x%08lX CAUGHT", (unsigned long)code);
        LogAddressInfo("[CRASH] Faulting Address: ", eip);

        if (code == EXCEPTION_ACCESS_VIOLATION && pExceptionInfo->ExceptionRecord->NumberParameters >= 2)
        {
            ULONG_PTR op = pExceptionInfo->ExceptionRecord->ExceptionInformation[0];
            ULONG_PTR badAddr = pExceptionInfo->ExceptionRecord->ExceptionInformation[1];
            VehicleHealthLogger::Log("[CRASH] Attempted to %s memory at 0x%08lX",
                                    op == 0 ? "READ" : (op == 1 ? "WRITE" : "EXECUTE"),
                                    (unsigned long)badAddr);
        }

        if (!IsBadReadPtr((const void*)eip, 16))
        {
            const unsigned char* bytes = (const unsigned char*)eip;
            char hexBuf[64] = {0};
            for (int i = 0; i < 16; i++)
            {
                sprintf(hexBuf + i * 3, "%02X ", bytes[i]);
            }
            VehicleHealthLogger::Log("[CRASH] Code bytes at EIP: %s", hexBuf);
        }

        VehicleHealthLogger::Log("[CRASH] Registers:");
        VehicleHealthLogger::Log("[CRASH]   EAX=0x%08lX EBX=0x%08lX ECX=0x%08lX EDX=0x%08lX", 
                                (unsigned long)eax, (unsigned long)ebx, (unsigned long)ecx, (unsigned long)edx);
        VehicleHealthLogger::Log("[CRASH]   ESI=0x%08lX EDI=0x%08lX EBP=0x%08lX ESP=0x%08lX", 
                                (unsigned long)esi, (unsigned long)edi, (unsigned long)ebp, (unsigned long)esp);

        uintptr_t* stack = (uintptr_t*)esp;
        if (!IsBadReadPtr(stack, 128))
        {
            VehicleHealthLogger::Log("[CRASH] Stack dump (ESP):");
            for (int i = 0; i < 32; i++)
            {
                char prefix[32];
                sprintf(prefix, "[CRASH]   [ESP+0x%02X] = ", i * 4);
                LogAddressInfo(prefix, stack[i]);
            }
        }
        VehicleHealthLogger::Log("==================================================");
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

namespace VehicleHealthLogger
{
    void Init(const char* logFilename)
    {
        if (s_LogFile)
        {
            fclose(s_LogFile);
            s_LogFile = nullptr;
        }

        char fullPath[MAX_PATH] = {0};
        GetModuleFileNameA(NULL, fullPath, MAX_PATH);
        char* lastSlash = strrchr(fullPath, '\\');
        if (!lastSlash) lastSlash = strrchr(fullPath, '/');

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
        if (!s_LogFile)
        {
            s_LogFile = fopen("NFSU2VehicleHealth.log", "w");
        }
        if (s_LogFile)
        {
            setvbuf(s_LogFile, NULL, _IONBF, 0);
            Log("==================================================");
            Log("  NFSU2 Vehicle Health & Damage Mod v1.0         ");
            Log("  Log Path: %s", fullPath);
            Log("==================================================");
        }

        AddVectoredExceptionHandler(1, VehicleHealth_VectoredCrashHandler);
    }

    void Log(const char* fmt, ...)
    {
        char buffer[1024] = {0};
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer) - 1, fmt, args);
        va_end(args);

        if (s_LogFile)
        {
            fprintf(s_LogFile, "%s\n", buffer);
            fflush(s_LogFile);
        }

        OutputDebugStringA("[NFSU2VehicleHealth] ");
        OutputDebugStringA(buffer);
        OutputDebugStringA("\n");
    }

    void Close()
    {
        if (s_LogFile)
        {
            fclose(s_LogFile);
            s_LogFile = nullptr;
        }
    }
}
