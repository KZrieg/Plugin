// main.cpp
#include <Windows.h>
#include "Console.h"
#include "hook.h"

DWORD WINAPI InitThread(LPVOID) {
    InitConsole();
    Log("[+] Plugin Injected.");
    InitializeHooks();   // 安装钩子，ImGui 初始化会在 hkPresent 中延迟执行
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}