// main.cpp
#include <Windows.h>
#include "Console.h"
#include "hook.h"

DWORD WINAPI InitThread(LPVOID) {
    InitializeHooks();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        InitConsole();
        Log("[DLL] Injected.");
        CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
    }
    return TRUE;
}