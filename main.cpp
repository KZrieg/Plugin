// hack/main.cpp
#include "hook.h"
#include "Bhop.h"
#include "Autostrafe.h"
#include "Console.h"
#include <Windows.h>
#include <thread>

DWORD WINAPI MainThread(LPVOID) {
    InitConsole();
    Log("[+] Plugin loaded");

    while (!GetModuleHandleW(L"client.dll")) {
        Sleep(100);
    }
    Log("[+] client.dll loaded");

    if (InstallCreateMoveHook()) {
        Log("[+] CreateMove hooked");
    }
    else {
        Log("[-] CreateMove hook failed");
    }

    Bhop::SetEnabled(true);
    Log("[+] Bhop enabled");

    Autostrafe::SetEnabled(false);
    Autostrafe::SetMode(AutostrafeMode::Normal);
    Log("[+] Autostrafe ready");

    while (!GetAsyncKeyState(VK_END)) {
        Sleep(50);
    }

    Bhop::SetEnabled(false);
    Autostrafe::SetEnabled(false);
    UninstallCreateMoveHook();
    Log("[+] Cleanup done");

    FreeConsole();
    FreeLibraryAndExitThread((HMODULE)GetModuleHandleW(L"Plugin.dll"), 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}