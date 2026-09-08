#include "hook.h"
#include "console.h"
#include "ui.h"
#include "vacexploit.h"          // for pattern_scan
#include "minhook/MinHook.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <cstdio>
#include "Signature.hpp"         // 特征码

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using CreateMoveFn = bool(__fastcall*)(void*, float, void*);

struct CUserCmd {
    uintptr_t vtable;
    int commandNumber;
    int tickCount;
    float viewAngles[3];
    float aimDirection[3];
    float forwardMove;
    float sideMove;
    float upMove;
    int buttons;
    int impulse;
    int weaponSelect;
    int weaponSubtype;
    int randomSeed;
    short mouseDx;
    short mouseDy;
    bool hasBeenPredicted;
};

static PresentFn g_origPresent = nullptr;
static CreateMoveFn g_origCreateMove = nullptr;

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* g_mainRTV = nullptr;
static HWND g_hwnd = nullptr;
static WNDPROC g_origWndProc = nullptr;

static bool g_imGuiInitialized = false;
static bool g_hooksInstalled = false;
bool g_showUI = false;

LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
bool HookCreateMove();
bool __fastcall hkCreateMove(void* pInput, float sampleTime, void* pCmd);

static bool GetPresentAddressFromTempSwapChain(void** ppPresent)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"TempWindowClass";
    if (!RegisterClassEx(&wc)) {
        Log("[-] RegisterClassEx failed");
        return false;
    }

    HWND hWnd = CreateWindowEx(0, L"TempWindowClass", L"", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hWnd) {
        Log("[-] CreateWindow failed");
        UnregisterClass(L"TempWindowClass", wc.hInstance);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    ID3D11Device* pDev = nullptr;
    ID3D11DeviceContext* pCtx = nullptr;
    IDXGISwapChain* pSwap = nullptr;

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr, 0,
        D3D11_SDK_VERSION,
        &sd,
        &pSwap,
        &pDev,
        &featureLevel,
        &pCtx
    );

    if (FAILED(hr)) {
        Log("[-] D3D11CreateDeviceAndSwapChain failed (hr=0x%X)", hr);
        DestroyWindow(hWnd);
        UnregisterClass(L"TempWindowClass", wc.hInstance);
        return false;
    }

    void** vtable = *(void***)pSwap;
    *ppPresent = vtable[8];

    Log("[+] Present address obtained: %p", *ppPresent);

    pSwap->Release();
    pDev->Release();
    pCtx->Release();
    DestroyWindow(hWnd);
    UnregisterClass(L"TempWindowClass", wc.hInstance);

    return true;
}

void InitializeImGui(IDXGISwapChain* pSwapChain)
{
    if (g_imGuiInitialized) return;

    Log("[*] Initializing ImGui...");

    HRESULT hr = pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
    if (FAILED(hr)) {
        Log("[-] Failed to get D3D11 device (hr=0x%X)", hr);
        return;
    }
    g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

    DXGI_SWAP_CHAIN_DESC sd;
    pSwapChain->GetDesc(&sd);
    g_hwnd = sd.OutputWindow;
    Log("[+] Window handle: %p", g_hwnd);

    ID3D11Texture2D* pBackBuffer = nullptr;
    pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRTV);
        pBackBuffer->Release();
    }

    g_origWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    if (!font) {
        Log("[-] Failed to load Microsoft YaHei, using default font");
        io.Fonts->AddFontDefault();
    }
    else {
        Log("[+] Microsoft YaHei font loaded");
    }

    io.IniFilename = nullptr;

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

    g_imGuiInitialized = true;
    Log("[+] ImGui initialized");
}

LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_showUI = !g_showUI;
        if (g_showUI) {
            ShowCursor(TRUE);
            RECT rect;
            GetWindowRect(g_hwnd, &rect);
            ClipCursor(&rect);
        }
        else {
            ShowCursor(FALSE);
            ClipCursor(nullptr);
        }
        return 1;
    }

    if (g_imGuiInitialized && g_showUI && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 1;

    if (g_imGuiInitialized && g_showUI) {
        switch (msg) {
        case WM_LBUTTONDOWN: case WM_LBUTTONUP:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_NCLBUTTONDOWN: case WM_NCLBUTTONUP:
        case WM_NCRBUTTONDOWN: case WM_NCRBUTTONUP:
        case WM_NCMBUTTONDOWN: case WM_NCMBUTTONUP:
        case WM_NCMOUSEMOVE:
        case WM_KEYDOWN: case WM_KEYUP:
        case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_CHAR:
            return 1;
        }
    }

    return CallWindowProc(g_origWndProc, hWnd, msg, wParam, lParam);
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    if (!g_imGuiInitialized) {
        InitializeImGui(pSwapChain);
    }

    if (g_imGuiInitialized) {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_showUI) {
            RenderUI();
        }

        ImGui::Render();

        if (g_mainRTV) {
            g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
        }
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    return g_origPresent(pSwapChain, SyncInterval, Flags);
}

bool HookCreateMove()
{
    HMODULE client = GetModuleHandleA("client.dll");
    if (!client) {
        Log("[-] client.dll not loaded");
        return false;
    }

    uintptr_t clientBase = (uintptr_t)client;

    const char* pattern = Signatures::CreateMove.data();
    uintptr_t found = pattern_scan(clientBase, pattern);
    if (!found) {
        Log("[-] CreateMove pattern not found");
        return false;
    }

    uintptr_t createMoveAddr = found + 28;
    Log("[+] CreateMove pattern found at %p, hooking at %p", (void*)found, (void*)createMoveAddr);

    DWORD oldProtect;
    VirtualProtect((void*)createMoveAddr, 16, PAGE_EXECUTE_READWRITE, &oldProtect);

    if (MH_CreateHook((void*)createMoveAddr, &hkCreateMove, (void**)&g_origCreateMove) != MH_OK) {
        Log("[-] MH_CreateHook(CreateMove) failed at %p", (void*)createMoveAddr);
        VirtualProtect((void*)createMoveAddr, 16, oldProtect, &oldProtect);
        return false;
    }

    if (MH_EnableHook((void*)createMoveAddr) != MH_OK) {
        Log("[-] MH_EnableHook(CreateMove) failed");
        VirtualProtect((void*)createMoveAddr, 16, oldProtect, &oldProtect);
        return false;
    }

    VirtualProtect((void*)createMoveAddr, 16, oldProtect, &oldProtect);
    Log("[+] CreateMove hooked at %p", (void*)createMoveAddr);
    return true;
}

bool __fastcall hkCreateMove(void* pInput, float sampleTime, void* pCmd)
{
    bool result = g_origCreateMove(pInput, sampleTime, pCmd);

    if (g_showUI && pCmd) {
        CUserCmd* cmd = (CUserCmd*)pCmd;
        cmd->mouseDx = 0;
        cmd->mouseDy = 0;
        cmd->buttons = 0;
        cmd->forwardMove = 0.0f;
        cmd->sideMove = 0.0f;
        cmd->upMove = 0.0f;
    }

    return result;
}

bool InitializeHack()
{
    Log("[*] Installing Present hook...");
    void* pPresent = nullptr;
    if (!GetPresentAddressFromTempSwapChain(&pPresent)) {
        Log("[-] Failed to obtain Present address");
        return false;
    }

    if (MH_CreateHook(pPresent, &hkPresent, (void**)&g_origPresent) != MH_OK) {
        Log("[-] MH_CreateHook(Present) failed");
        return false;
    }

    if (MH_EnableHook(pPresent) != MH_OK) {
        Log("[-] MH_EnableHook(Present) failed");
        return false;
    }
    Log("[+] Present hook installed");

    if (!HookCreateMove()) {
        Log("[-] CreateMove hook installation failed");
    }

    g_hooksInstalled = true;
    return true;
}

void ShutdownHack()
{
    if (g_hooksInstalled) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(g_origPresent);
        MH_RemoveHook(g_origCreateMove);
        g_hooksInstalled = false;
        Log("[+] Hooks removed");
    }

    if (g_imGuiInitialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        if (g_origWndProc) {
            SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
        }
        if (g_mainRTV) g_mainRTV->Release();
        if (g_pd3dContext) g_pd3dContext->Release();
        if (g_pd3dDevice) g_pd3dDevice->Release();
        g_imGuiInitialized = false;
        Log("[+] ImGui shutdown");
    }
}