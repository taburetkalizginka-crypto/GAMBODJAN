// GAMBODJAN DLL — Injected into dota2.exe
// Creates transparent overlay window on top of the game
// INSERT = toggle menu, END = unload
// FOV Changer only

#ifndef GAMBODJAN_WINDOWS
#define GAMBODJAN_WINDOWS
#endif

#include <Windows.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

#include "core/lib/imgui/imgui.h"
#include "core/lib/imgui/imgui_impl_win32.h"
#include "core/lib/imgui/imgui_impl_dx11.h"
#include "gui/menu.h"
#include "core/offsets.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static HMODULE                  g_hModule = nullptr;
static bool                     g_running = true;
static HWND                     g_overlay = nullptr;
static HWND                     g_gameWindow = nullptr;
static ID3D11Device*            g_device = nullptr;
static ID3D11DeviceContext*     g_context = nullptr;
static IDXGISwapChain*          g_swapChain = nullptr;
static ID3D11RenderTargetView*  g_rtv = nullptr;

// ================================================================
//  Print to Dota 2 in-game console via tier0.dll ConMsg
// ================================================================
static void PrintToGameConsole(const char* msg) {
    HMODULE tier0 = GetModuleHandleA("tier0.dll");
    if (!tier0) return;

    typedef void (*ConMsgFn)(const char* fmt, ...);
    auto ConMsg = (ConMsgFn)GetProcAddress(tier0, "ConMsg");
    if (ConMsg) {
        ConMsg("%s\n", msg);
    }
}

// ================================================================
//  Engine init — detect game, set status
// ================================================================
static bool g_engineInitialized = false;

static void InitEngine() {
    if (g_engineInitialized) return;
    g_engineInitialized = true;

    HMODULE engine2 = GetModuleHandleA("engine2.dll");
    if (!engine2) {
        snprintf(menu::state.status_text, sizeof(menu::state.status_text),
            "engine2.dll not loaded yet");
        return;
    }

    typedef void* (*CreateInterfaceFn)(const char* name, int* ret);
    auto factory = (CreateInterfaceFn)GetProcAddress(engine2, "CreateInterface");
    if (!factory) {
        snprintf(menu::state.status_text, sizeof(menu::state.status_text),
            "CreateInterface not found");
        return;
    }

    int ret = 0;
    void* engineClient = factory("Source2EngineToClient001", &ret);
    if (engineClient) {
        menu::state.game_found = true;
        snprintf(menu::state.status_text, sizeof(menu::state.status_text),
            "Engine interface found!");
        return;
    }

    snprintf(menu::state.status_text, sizeof(menu::state.status_text),
        "Engine found, using command mode");
    menu::state.game_found = true;
}

// ================================================================
//  Find Dota 2 game window
// ================================================================
static HWND FindGameWindow() {
    HWND hwnd = nullptr;
    DWORD pid = GetCurrentProcessId();

    do {
        hwnd = FindWindowExW(nullptr, hwnd, nullptr, nullptr);
        if (!hwnd) break;
        DWORD windowPid = 0;
        GetWindowThreadProcessId(hwnd, &windowPid);
        if (windowPid == pid && IsWindowVisible(hwnd)) {
            RECT r;
            GetWindowRect(hwnd, &r);
            int w = r.right - r.left;
            int h = r.bottom - r.top;
            if (w > 640 && h > 480) {
                return hwnd;
            }
        }
    } while (hwnd);

    return nullptr;
}

// ================================================================
//  Create DX11 device for overlay
// ================================================================
static bool CreateOverlayDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &fl, &g_context);
    if (FAILED(hr)) return false;

    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
        backBuffer->Release();
    }
    return true;
}

static void CleanupDevice() {
    if (g_rtv)       { g_rtv->Release();       g_rtv = nullptr; }
    if (g_swapChain) { g_swapChain->Release();  g_swapChain = nullptr; }
    if (g_context)   { g_context->Release();    g_context = nullptr; }
    if (g_device)    { g_device->Release();     g_device = nullptr; }
}

// ================================================================
//  Overlay WndProc
// ================================================================
static LRESULT WINAPI OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

// ================================================================
//  Main overlay thread
// ================================================================
static DWORD WINAPI OverlayThread(LPVOID) {
    // Wait for game window
    int attempts = 0;
    while (!g_gameWindow && g_running && attempts < 120) {
        g_gameWindow = FindGameWindow();
        Sleep(500);
        attempts++;
    }
    if (!g_running || !g_gameWindow) return 0;

    Sleep(2000);

    RECT gameRect;
    GetWindowRect(g_gameWindow, &gameRect);
    int w = gameRect.right - gameRect.left;
    int h = gameRect.bottom - gameRect.top;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = g_hModule;
    wc.lpszClassName = L"GAMBODJAN_OVERLAY";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    g_overlay = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        wc.lpszClassName, L"GAMBODJAN Overlay",
        WS_POPUP,
        gameRect.left, gameRect.top, w, h,
        nullptr, nullptr, g_hModule, nullptr);

    if (!g_overlay) return 1;

    SetLayeredWindowAttributes(g_overlay, RGB(0, 0, 0), 0, LWA_COLORKEY);
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_overlay, &margins);

    ShowWindow(g_overlay, SW_SHOWNOACTIVATE);
    UpdateWindow(g_overlay);

    if (!CreateOverlayDevice(g_overlay)) {
        DestroyWindow(g_overlay);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplWin32_Init(g_overlay);
    ImGui_ImplDX11_Init(g_device, g_context);
    menu::ApplyStyle();

    // Detect engine
    InitEngine();

    // Print to Dota 2 in-game console
    PrintToGameConsole("GAMBODJAN CLAN");

    snprintf(menu::state.status_text, sizeof(menu::state.status_text),
        "Overlay active. Menu: INSERT");

    // Main render loop
    while (g_running) {
        // Hotkeys
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            menu::Toggle();
            if (menu::state.show) {
                LONG exStyle = GetWindowLong(g_overlay, GWL_EXSTYLE);
                exStyle &= ~WS_EX_TRANSPARENT;
                SetWindowLong(g_overlay, GWL_EXSTYLE, exStyle);
                SetForegroundWindow(g_overlay);
            } else {
                LONG exStyle = GetWindowLong(g_overlay, GWL_EXSTYLE);
                exStyle |= WS_EX_TRANSPARENT;
                SetWindowLong(g_overlay, GWL_EXSTYLE, exStyle);
                SetForegroundWindow(g_gameWindow);
                if (menu::state.save_on_close)
                    menu::SaveConfig(menu::state.config_name);
            }
        }

        if (GetAsyncKeyState(VK_END) & 1) {
            g_running = false;
            break;
        }

        // Track game window position/size
        if (g_gameWindow && IsWindow(g_gameWindow)) {
            RECT gr;
            GetWindowRect(g_gameWindow, &gr);
            int nw = gr.right - gr.left;
            int nh = gr.bottom - gr.top;
            if (gr.left != gameRect.left || gr.top != gameRect.top || nw != w || nh != h) {
                gameRect = gr;
                w = nw;
                h = nh;
                MoveWindow(g_overlay, gr.left, gr.top, w, h, TRUE);
            }
        } else {
            g_running = false;
            break;
        }

        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (menu::state.show) {
            menu::Render();
        }

        ImGui::Render();
        const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_context->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_swapChain->Present(1, 0);
        Sleep(1);
    }

    // Cleanup
    if (menu::state.save_on_close)
        menu::SaveConfig(menu::state.config_name);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDevice();
    DestroyWindow(g_overlay);
    UnregisterClassW(wc.lpszClassName, g_hModule);

    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, OverlayThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
