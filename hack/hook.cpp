// hook.cpp
#include "hook.h"
#include "offsets.hpp"
#include "Bhop.h"
#include "Autostrafe.h"
#include "Console.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "ui.h"
#include <MinHook.h>
#include <d3d11.h>
#include <thread>
#include <chrono>

using namespace cs2_dumper::offsets::client_dll;

// ---------- 基础结构体 ----------
struct Vector {
    float x, y, z;
};

struct QAngle {
    float x, y, z;
};

// ---------- CUserCmd ----------
struct CUserCmd {
    int command_number;
    int tick_count;
    QAngle viewangles;
    Vector aimdirection;
    float forwardmove;
    float sidemove;
    float upmove;
    int buttons;
    uint64_t nButtons;
    uint64_t nValueScroll;
    uint64_t nValueChanged;
    void* pBaseCmd;
};

// ---------- 全局变量 ----------
bool g_showMenu = true;
static bool g_initialized = false;

static ID3D11Device* g_pd3dDevice = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* g_mainRTV = nullptr;
static HWND g_hwnd = nullptr;
static WNDPROC g_origWndProc = nullptr;

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
static PresentFn g_origPresent = nullptr;

using CreateMoveFn = bool(__fastcall*)(void*, int, void*);
static CreateMoveFn g_origCreateMove = nullptr;

// ---------- 前置声明 ----------
LRESULT __stdcall WndProc(HWND, UINT, WPARAM, LPARAM);

// ---------- 获取 CSGOInput ----------
void* GetCSGOInputInstance() {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return nullptr;
    return (void*)(client + dwCSGOInput);
}

// ---------- CreateMove 钩子（最终版） ----------
bool __fastcall hkCreateMove(void* pCSGOInput, int nSlot, void* bActive) {
    bool bResult = g_origCreateMove(pCSGOInput, nSlot, bActive);

    constexpr std::ptrdiff_t COMMAND_OFFSET = 0x2C;
    CUserCmd* pUserCmd = *(CUserCmd**)((uintptr_t)pCSGOInput + COMMAND_OFFSET);
    if (pUserCmd) {
        DoBhopCmd(pUserCmd);
        DoAutostrafeCmd(pUserCmd);
    }

    return bResult;
}

// ---------- Present 钩子 ----------
HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT syncInterval, UINT flags) {
    if (!g_initialized) {
        pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
        g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

        DXGI_SWAP_CHAIN_DESC sd;
        pSwapChain->GetDesc(&sd);
        g_hwnd = sd.OutputWindow;

        ID3D11Texture2D* pBackBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        if (pBackBuffer) {
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRTV);
            pBackBuffer->Release();
        }

        g_pSwapChain = pSwapChain;
        g_pSwapChain->AddRef();

        g_origWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 18.0f, nullptr,
            io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        if (font) io.FontDefault = font;
        else io.Fonts->AddFontDefault();

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);

        g_initialized = true;
        Log("[ImGui] Initialized.");
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_showMenu) {
        RenderUI(&g_showMenu);
    }

    ImGui::EndFrame();
    ImGui::Render();
    g_pd3dContext->OMSetRenderTargets(1, &g_mainRTV, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return g_origPresent(pSwapChain, syncInterval, flags);
}

// ---------- 窗口过程 ----------
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT __stdcall WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_showMenu = !g_showMenu;
        return 0;
    }
    if (g_initialized && ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;
    return CallWindowProc(g_origWndProc, hwnd, uMsg, wParam, lParam);
}

// ---------- 安装 Present 钩子 ----------
static bool InstallPresentHook() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0, 0,
                      GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                      L"Temp", nullptr };
    RegisterClassEx(&wc);
    HWND hTempWnd = CreateWindow(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
        0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hTempWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ID3D11Device* pTempDevice = nullptr;
    IDXGISwapChain* pTempSwapChain = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
        &sd, &pTempSwapChain, &pTempDevice, nullptr, nullptr);

    if (FAILED(hr) || !pTempSwapChain) {
        Log("[Hook] Failed to create temp D3D11 device.");
        DestroyWindow(hTempWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(pTempSwapChain);
    constexpr int PRESENT_VTABLE_INDEX = 8;
    void* presentAddr = vtable[PRESENT_VTABLE_INDEX];

    MH_STATUS status = MH_CreateHook(presentAddr, hkPresent, (void**)&g_origPresent);
    if (status != MH_OK) {
        Log("[Hook] MH_CreateHook Present failed: %s", MH_StatusToString(status));
        pTempDevice->Release();
        pTempSwapChain->Release();
        DestroyWindow(hTempWnd);
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }

    status = MH_EnableHook(presentAddr);
    if (status != MH_OK) {
        Log("[Hook] MH_EnableHook Present failed: %s", MH_StatusToString(status));
    }
    else {
        Log("[Hook] Present hook installed.");
    }

    pTempDevice->Release();
    pTempSwapChain->Release();
    DestroyWindow(hTempWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return true;
}

// ---------- 安装 CreateMove 钩子 ----------
static bool InstallCreateMoveHook() {
    void* pInput = GetCSGOInputInstance();
    if (!pInput) {
        Log("[Hook] CSGOInput instance not found.");
        return false;
    }

    void** pVTable = *reinterpret_cast<void***>(pInput);
    if (!pVTable) {
        Log("[Hook] CSGOInput VTable is null.");
        return false;
    }

    constexpr int CREATE_MOVE_INDEX = 5;
    void* createMoveAddr = pVTable[CREATE_MOVE_INDEX];
    if (!createMoveAddr) {
        Log("[Hook] CreateMove VTable entry is null.");
        return false;
    }

    Log("[Hook] CreateMove found at 0x%p (VTable[%d])", createMoveAddr, CREATE_MOVE_INDEX);

    MH_STATUS status = MH_CreateHook(createMoveAddr, hkCreateMove, (void**)&g_origCreateMove);
    if (status != MH_OK) {
        Log("[Hook] MH_CreateHook CreateMove failed: %s", MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(createMoveAddr);
    if (status != MH_OK) {
        Log("[Hook] MH_EnableHook CreateMove failed: %s", MH_StatusToString(status));
        return false;
    }

    Log("[Hook] CreateMove hook installed.");
    return true;
}

// ---------- 公开初始化 ----------
void InitializeHooks() {
    Log("[Hook] Waiting for client.dll...");
    while (!GetModuleHandleA("client.dll")) Sleep(100);
    Log("[Hook] client.dll loaded.");

    Log("[Hook] Waiting for CSGOInput...");
    while (!GetCSGOInputInstance()) Sleep(100);
    Log("[Hook] CSGOInput found.");

    if (MH_Initialize() != MH_OK) {
        Log("[Hook] MH_Initialize failed.");
        return;
    }

    if (!InstallPresentHook()) {
        Log("[Hook] Failed to install Present hook.");
    }

    if (!InstallCreateMoveHook()) {
        Log("[Hook] Failed to install CreateMove hook.");
    }

    Log("[Hook] All hooks initialized.");
}