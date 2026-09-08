#include "hack/hook.h"
#include "hack/console.h"
#include "hack/vacexploit.h"
#include "minhook/MinHook.h"

DWORD WINAPI MainThread(LPVOID)
{
    InitConsole();
    Log("[+] Plugin loaded");

    while (!GetModuleHandleW(L"client.dll")) {
        Sleep(100);
    }
    Log("[+] client.dll loaded");

    if (MH_Initialize() != MH_OK) {
        Log("[-] MinHook init failed");
        return 0;
    }

    if (!InitializeHack()) {
        Log("[-] Hack (Present hook) init failed");
    }
    else {
        Log("[+] Present hooked");
    }

    if (VACExploit_Init()) {
        Log("[+] VAC Exploit ready");
    }
    else {
        Log("[-] VAC Exploit init failed");
    }

    Log("[*] All systems ready");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}