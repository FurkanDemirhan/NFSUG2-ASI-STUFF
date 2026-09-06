#pragma once
#include <windows.h>
#include <cstdint>

// Memory addresses and function pointers for NFSUG2 PC v1.2 NTSC (SPEED2.EXE, 4,800,512 bytes)

namespace GameAddresses
{
    // Global Variables
    constexpr uintptr_t TheGameFlowManager = 0x8654A4;   // 3 = Frontend, 4/5 = Loading, 6 = Gameplay
    constexpr uintptr_t WindowHandle       = 0x870990;   // HWND
    constexpr uintptr_t IsLostFocus        = 0x8709E0;   // bool
    constexpr uintptr_t PlayersByNumber    = 0x8900AC;   // Pointer to player array
    constexpr uintptr_t UnlockAllThings    = 0x838464;   // unsigned char
    constexpr uintptr_t MainLoopHook       = 0x581470;   // call __return (0x4022C0)

    // Functions
    inline uint32_t (*bStringHash)(const char* str) = (uint32_t (*)(const char*))0x43DB50;
    inline int (*FEHashUpper)(const char* str) = (int (*)(const char*))0x505450;
    inline void* (*j__malloc)(size_t size) = (void* (*)(size_t))0x575620;

    // Menu functions
    inline int (__thiscall* AddElementToMenuWithStateID)(int _this, int a2, int a3, int a4, int a5) 
        = (int (__thiscall*)(int, int, int, int, int))0x520CB0;

    inline void* (__thiscall* AddCustomElementToMenu)(void* _this, int a2, int a3, int a4) 
        = (void* (__thiscall*)(void*, int, int, int))0x545920;

    // Player / Autopilot functions
    inline int (__thiscall* Player_AutoPilotOn)(DWORD* player) = (int (__thiscall*)(DWORD*))0x5FAE20;
    inline int (__thiscall* Player_AutoPilotOff)(DWORD* player) = (int (__thiscall*)(DWORD*))0x5FAF10;
    inline void (__thiscall* Player_SetInputMode)(void* player, int mode) = (void (__thiscall*)(void*, int))0x605C70;
}
