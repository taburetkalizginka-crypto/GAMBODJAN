// GAMBODJAN — Launcher EXE with ImGui menu + built-in DLL injector
// Build as Windows GUI application

#include <Windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <TlHelp32.h>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

#include "core/lib/imgui/imgui.h"
#include "core/lib/imgui/imgui_impl_win32.h"
#include "core/lib/imgui/imgui_impl_dx11.h"
#include "gui/menu.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ================================================================
//  Injector
// ================================================================
namespace injector {

    enum class Status {
        Idle,
        Searching,
        Found,
        Injecting,
        Success,
        Error
    };

    struct State {
        Status status = Status::Idle;
        char   dll_path[512] = {};
        char   status_text[256] = "Ready";
        DWORD  target_pid = 0;
        bool   auto_inject = false;
    };

    inline State g_state;

    static DWORD FindProcessByName(const wchar_t* name) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W pe = {};
        pe.dwSize = sizeof(pe);
        DWORD pid = 0;

        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, name) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }

        CloseHandle(snap);
        return pid;
    }

    static bool InjectDLL(DWORD pid, const char* dllPath) {
        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProcess) return false;

        char fullPath[MAX_PATH] = {};
        GetFullPathNameA(dllPath, MAX_PATH, fullPath, nullptr);

        size_t pathLen = strlen(fullPath) + 1;
        LPVOID remoteMem = VirtualAllocEx(hProcess, nullptr, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remoteMem) {
            CloseHandle(hProcess);
            return false;
        }

        if (!WriteProcessMemory(hProcess, remoteMem, fullPath, pathLen, nullptr)) {
            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
        FARPROC pLoadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");

        HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
            (LPTHREAD_START_ROUTINE)pLoadLibrary, remoteMem, 0, nullptr);

        if (!hThread) {
            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
            CloseHandle(hProcess);
            return false;
        }

        WaitForSingleObject(hThread, 5000);

        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);

        CloseHandle(hThread);
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);

        return exitCode != 0;
    }

    static void DoInject() {
        auto& s = g_state;

        if (s.dll_path[0] == '\0') {
            s.status = Status::Error;
            strcpy_s(s.status_text, "Error: DLL path is empty");
            return;
        }

        s.status = Status::Searching;
        strcpy_s(s.status_text, "Searching for dota2.exe...");

        s.target_pid = FindProcessByName(L"dota2.exe");
        if (s.target_pid == 0) {
            s.status = Status::Error;
            strcpy_s(s.status_text, "Error: dota2.exe not found. Launch Dota 2 first!");
            return;
        }

        s.status = Status::Injecting;
        sprintf_s(s.status_text, "Found dota2.exe (PID: %lu). Injecting...", s.target_pid);

        if (InjectDLL(s.target_pid, s.dll_path)) {
            s.status = Status::Success;
            sprintf_s(s.status_text, "Injected successfully! PID: %lu", s.target_pid);
        } else {
            s.status = Status::Error;
            strcpy_s(s.status_text, "Error: Injection failed. Run as Administrator!");
        }
    }
}

// ================================================================
//  DirectX 11
// ================================================================
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain)        { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release();  g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)        { g_pd3dDevice->Release();         g_pd3dDevice = nullptr; }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

// ================================================================
//  Injector panel (rendered inside the menu)
// ================================================================
static void RenderInjectorPanel() {
    auto& inj = injector::g_state;

    static const ImVec4 col_accent   = ImVec4(0.80f, 0.18f, 0.18f, 1.00f);
    static const ImVec4 col_green    = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    static const ImVec4 col_red      = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
    static const ImVec4 col_yellow   = ImVec4(0.9f, 0.8f, 0.2f, 1.0f);
    static const ImVec4 col_text_dim = ImVec4(0.55f, 0.55f, 0.60f, 1.0f);

    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("GAMBODJAN Injector", nullptr, ImGuiWindowFlags_NoCollapse);
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_accent);
        ImGui::TextUnformatted("GAMBODJAN Launcher");
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Spacing();

        // DLL path input
        ImGui::TextUnformatted("DLL Path:");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
        ImGui::InputText("##dll_path", inj.dll_path, sizeof(inj.dll_path));
        ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(72, 0))) {
            OPENFILENAMEA ofn = {};
            char szFile[512] = {};
            if (inj.dll_path[0]) strcpy_s(szFile, inj.dll_path);
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "DLL Files\0*.dll\0All Files\0*.*\0";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
            if (GetOpenFileNameA(&ofn)) {
                strcpy_s(inj.dll_path, szFile);
            }
        }

        ImGui::Spacing();

        // Process status
        DWORD pid = injector::FindProcessByName(L"dota2.exe");
        if (pid) {
            ImGui::TextColored(col_green, "dota2.exe found (PID: %lu)", pid);
        } else {
            ImGui::TextColored(col_red, "dota2.exe not found - launch Dota 2 first");
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // Inject button
        bool canInject = (pid != 0 && inj.dll_path[0] != '\0' &&
                          inj.status != injector::Status::Success);

        if (!canInject) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            ImGui::Button("INJECT INTO DOTA 2", ImVec2(ImGui::GetContentRegionAvail().x, 40));
            ImGui::PopStyleVar();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, col_accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.28f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
            if (ImGui::Button("INJECT INTO DOTA 2", ImVec2(ImGui::GetContentRegionAvail().x, 40))) {
                injector::DoInject();
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::Spacing();

        // Status message
        ImVec4 statusColor = col_text_dim;
        if (inj.status == injector::Status::Success)   statusColor = col_green;
        if (inj.status == injector::Status::Error)      statusColor = col_red;
        if (inj.status == injector::Status::Injecting)  statusColor = col_yellow;
        if (inj.status == injector::Status::Searching)  statusColor = col_yellow;
        ImGui::TextColored(statusColor, "%s", inj.status_text);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(col_text_dim, "Instructions:");
        ImGui::BulletText("1. Launch Dota 2 via Steam");
        ImGui::BulletText("2. Browse and select GAMBODJAN.dll");
        ImGui::BulletText("3. Click INJECT INTO DOTA 2");
        ImGui::BulletText("4. Press INSERT in-game to open menu");
        ImGui::Spacing();
        ImGui::TextColored(col_text_dim, "Run this launcher as Administrator for best results");
    }
    ImGui::End();
}

// ================================================================
//  WinMain
// ================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"GAMBODJAN_CLASS";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName, L"GAMBODJAN Launcher",
        WS_OVERLAPPEDWINDOW,
        100, 100, 820, 640,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    menu::state.show = true;
    menu::ApplyStyle();

    ImVec4 clear_color = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Render injector panel only (menu is inside the game DLL)
        RenderInjectorPanel();

        ImGui::Render();
        const float cc[4] = { clear_color.x, clear_color.y, clear_color.z, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
