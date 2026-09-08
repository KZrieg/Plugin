#include "hook.h"
#include "console.h"
#include "ui.h"
#include "MinHook.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <cstdio>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
static PresentFn g_origPresent = nullptr;

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* g_mainRTV = nullptr;
static HWND g_hwnd = nullptr;
static WNDPROC g_origWndProc = nullptr;
static bool g_imGuiInitialized = false;
static bool g_hooksInstalled = false;
static bool g_showUI = true;

LRESULT __stdcall WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
        return 1;
    }

    if (g_imGuiInitialized && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 1;

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

bool InitializeHack()
{
    Log("[*] Installing Present hook...");

    void* pPresent = nullptr;
    if (!GetPresentAddressFromTempSwapChain(&pPresent)) {
        Log("[-] Failed to obtain Present address");
        return false;
    }

    if (MH_CreateHook(pPresent, &hkPresent, (void**)&g_origPresent) != MH_OK) {
        Log("[-] MH_CreateHook failed");
        return false;
    }

    if (MH_EnableHook(pPresent) != MH_OK) {
        Log("[-] MH_EnableHook failed");
        return false;
    }

    g_hooksInstalled = true;
    Log("[+] Present hook installed");
    return true;
}

void ShutdownHack()
{
    if (g_hooksInstalled) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_RemoveHook(g_origPresent);
        g_hooksInstalled = false;
        Log("[+] Present hook removed");
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