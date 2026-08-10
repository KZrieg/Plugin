#include <Windows.h>
#include <d3d11.h>
#include <MinHook.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <thread>
#include <chrono>

#include "Console.h"
#include "ui.h"
#include "Bhop.h"
#include "Autostrafe.h"
#include "offsets.hpp"
#include "interfaces.hpp"

// ---------- ImGui 渲染全局 ----------
static ID3D11Device* g_pd3dDevice = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* g_mainRTV = nullptr;
static HWND                     g_hwnd = nullptr;
static WNDPROC                  g_origWndProc = nullptr;
static bool                     g_initialized = false;
static bool                     g_showMenu = true;

static void* g_originPresent = nullptr;
using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
static PresentFn                g_origPresentFn = nullptr;

// ---------- CreateMove Hook ----------
typedef void(__fastcall* CreateMoveFn)(void* thisptr, int sequence_number, float input_sample_frametime, bool bActive);
static CreateMoveFn             g_origCreateMove = nullptr;

void* GetCSGOInputInstance() {
    uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
    if (!client) return nullptr;
    uintptr_t pInputPtr = client + cs2_dumper::offsets::client_dll::dwCSGOInput;
    return *reinterpret_cast<void**>(pInputPtr);
}

void* GetCreateMoveByVTable() {
    void* pInput = GetCSGOInputInstance();
    if (!pInput) return nullptr;
    uintptr_t* pVTable = *reinterpret_cast<uintptr_t**>(pInput);
    if (!pVTable) return nullptr;
    constexpr int kCreateMoveVTableIndex = 25;  // 根据实际版本调整
    return reinterpret_cast<void*>(pVTable[kCreateMoveVTableIndex]);
}

void __fastcall hkCreateMove(void* thisptr, int sequence_number, float input_sample_frametime, bool bActive) {
    g_origCreateMove(thisptr, sequence_number, input_sample_frametime, bActive);

    // 注意：此处需获取 CUserCmd*，若您能直接获得，请调用 DoBhopCmd(cmd) 和 DoAutostrafeCmd(cmd)
    // 若无法获取，可暂不调用，避免编译错误。
    // 例如：
    // CUserCmd* cmd = GetCurrentCmd(); // 需您实现
    // if (cmd) {
    //     DoBhopCmd(cmd);
    //     DoAutostrafeCmd(cmd);
    // }
}

// ---------- ImGui 窗口过程 ----------
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

// ---------- Present Hook ----------
HRESULT __stdcall my_present(IDXGISwapChain* _this, UINT syncInterval, UINT flags) {
    if (!g_initialized) {
        _this->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
        g_pd3dDevice->GetImmediateContext(&g_pd3dContext);

        DXGI_SWAP_CHAIN_DESC sd;
        _this->GetDesc(&sd);
        g_hwnd = sd.OutputWindow;

        ID3D11Texture2D* backBuffer = nullptr;
        _this->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        if (backBuffer) {
            g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRTV);
            backBuffer->Release();
        }

        g_pSwapChain = _this;
        g_pSwapChain->AddRef();

        g_origWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        ImFont* font = io.Fonts->AddFontFromFileTTF(
            "C:\\Windows\\Fonts\\msyh.ttc", 18.0f, nullptr,
            io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
        if (font)
            io.FontDefault = font;
        else
            io.Fonts->AddFontDefault();

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

    return g_origPresentFn(_this, syncInterval, flags);
}

// ---------- Hook 安装线程 ----------
DWORD WINAPI InitHookThread(LPVOID) {
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

    if (SUCCEEDED(hr) && pTempSwapChain) {
        void** vtable = *reinterpret_cast<void***>(pTempSwapChain);
        void* presentAddr = vtable[8];

        MH_Initialize();
        MH_CreateHook(presentAddr, my_present, &g_originPresent);
        MH_EnableHook(presentAddr);
        g_origPresentFn = (PresentFn)g_originPresent;
        Log("[Hook] Present installed.");

        void* createMove = GetCreateMoveByVTable();
        if (createMove) {
            MH_CreateHook(createMove, hkCreateMove, (void**)&g_origCreateMove);
            MH_EnableHook(createMove);
            Log("[Hook] CreateMove installed via VTable.");
        }
        else {
            Log("[Hook] Failed to get CreateMove via VTable.");
        }

        pTempDevice->Release();
        pTempSwapChain->Release();
    }
    else {
        Log("[Hook] Failed to create temporary D3D11 device.");
    }

    DestroyWindow(hTempWnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return 0;
}

// ---------- 功能线程（备用） ----------
void FeatureLoop() {
    Log("[Feature] Thread started.");
    while (true) {
        // 若未在 CreateMove 中实现，可在此调用 DoBhop() 等
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// ---------- DLL 入口 ----------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        InitConsole();
        Log("[DLL] Injected.");
        CreateThread(nullptr, 0, InitHookThread, nullptr, 0, nullptr);
        // std::thread(FeatureLoop).detach(); // 可选
    }
    return TRUE;
}