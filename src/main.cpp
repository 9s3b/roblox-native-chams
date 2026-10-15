// champoc - native chams external.
// Chams ported from thenwefuckin (https://discord.gg/thenwefuckin).

#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <cstdio>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "game.hpp"
#include "chams_native.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "winmm.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
HWND                    g_hwnd = NULL;
WNDCLASSEX              g_wc = {};
ID3D11Device*           g_dev = nullptr;
ID3D11DeviceContext*    g_ctx = nullptr;
IDXGISwapChain1*        g_swap = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
IDCompositionDevice*    g_dcomp = nullptr;
IDCompositionTarget*    g_target = nullptr;
IDCompositionVisual*    g_visual = nullptr;
bool                    g_menuOpen = true;

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(h, m, w, l)) return true;
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (m == WM_SIZE && g_dev && w != SIZE_MINIMIZED) {
        if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
        g_swap->ResizeBuffers(0, (UINT)LOWORD(l), (UINT)HIWORD(l), DXGI_FORMAT_UNKNOWN, 0);
        ID3D11Texture2D* bb = nullptr;
        g_swap->GetBuffer(0, IID_PPV_ARGS(&bb));
        if (bb) { g_dev->CreateRenderTargetView(bb, nullptr, &g_rtv); bb->Release(); }
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

bool OverlayInit() {
    timeBeginPeriod(1);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    nc::g_screenW = sw; nc::g_screenH = sh;

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "champoc";
    if (!RegisterClassExA(&wc)) return false;
    g_wc = wc;

    g_hwnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        wc.lpszClassName, "champoc", WS_POPUP, 0, 0, sw, sh, NULL, NULL, wc.hInstance, NULL);
    if (!g_hwnd) return false;

    MARGINS m = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(g_hwnd, &m);
    DWM_BLURBEHIND bb = {}; bb.dwFlags = DWM_BB_ENABLE; bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(g_hwnd, &bb);
    SetLayeredWindowAttributes(g_hwnd, 0, 255, LWA_ALPHA);
    ShowWindow(g_hwnd, SW_SHOW);

    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &g_dev, &fl, &g_ctx)))
        return false;

    IDXGIDevice* dxgiDev = nullptr;
    if (FAILED(g_dev->QueryInterface(IID_PPV_ARGS(&dxgiDev)))) return false;
    IDXGIAdapter* adapter = nullptr;
    if (FAILED(dxgiDev->GetAdapter(&adapter))) { dxgiDev->Release(); return false; }
    IDXGIFactory2* factory = nullptr;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) { adapter->Release(); dxgiDev->Release(); return false; }

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = sw; scd.Height = sh; scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1; scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2; scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    scd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; scd.Scaling = DXGI_SCALING_STRETCH;
    HRESULT hr = factory->CreateSwapChainForComposition(g_dev, &scd, nullptr, &g_swap);
    factory->Release();
    if (FAILED(hr)) { adapter->Release(); dxgiDev->Release(); return false; }
    adapter->Release();

    if (FAILED(DCompositionCreateDevice(dxgiDev, IID_PPV_ARGS(&g_dcomp)))) { dxgiDev->Release(); return false; }
    dxgiDev->Release();
    if (FAILED(g_dcomp->CreateTargetForHwnd(g_hwnd, TRUE, &g_target))) return false;
    if (FAILED(g_dcomp->CreateVisual(&g_visual))) return false;
    if (FAILED(g_visual->SetContent(g_swap))) return false;
    if (FAILED(g_target->SetRoot(g_visual))) return false;
    if (FAILED(g_dcomp->Commit())) return false;

    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    g_dev->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);
    return true;
}

bool OverlayBegin() {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
        if (msg.message == WM_QUIT) return false;
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    return true;
}

void OverlayEnd() {
    ImGui::Render();
    float clear[4] = { 0, 0, 0, 0 };
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swap->Present(0, 0);
}

void OverlayShutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (g_visual) g_visual->Release();
    if (g_target) g_target->Release();
    if (g_dcomp)  g_dcomp->Release();
    if (g_rtv)    g_rtv->Release();
    if (g_swap)   g_swap->Release();
    if (g_ctx)    g_ctx->Release();
    if (g_dev)    g_dev->Release();
    if (g_hwnd)   DestroyWindow(g_hwnd);
    timeEndPeriod(1);
}

void MatchRobloxWindow(HWND target) {
    if (!target || !g_hwnd) return;
    RECT cr;
    if (!GetClientRect(target, &cr)) return;
    POINT tl = { cr.left, cr.top };
    ClientToScreen(target, &tl);
    SetWindowPos(g_hwnd, HWND_TOPMOST, tl.x, tl.y,
                 cr.right - cr.left, cr.bottom - cr.top, SWP_NOACTIVATE);
}

void SetClickThrough(bool on) {
    LONG_PTR ex = GetWindowLongPtrA(g_hwnd, GWL_EXSTYLE);
    if (on) ex |= WS_EX_TRANSPARENT; else ex &= ~WS_EX_TRANSPARENT;
    SetWindowLongPtrA(g_hwnd, GWL_EXSTYLE, ex);
}

const char* const kStyles[] = {
    "Flat", "Lit", "Clay", "Glass", "Crystal", "Ice", "Prism", "Heat", "Core",
    "flat", "chrome", "rainbow", "pearl", "glossy",
    "holographic", "fade", "wireframe", "glass2", "ropes",
    "liquid metal", "plasma", "gold", "ice2", "toxic",
    "galaxy", "matrix"
};

void DrawMenu() {
    if (!g_menuOpen) return;
    ImGui::SetNextWindowSize(ImVec2(330, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("champoc", &g_menuOpen, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("Enable chams", &g_chams.enabled);
    ImGui::Combo("Style", &g_chams.shader, kStyles, (int)(sizeof(kStyles) / sizeof(kStyles[0])));
    ImGui::ColorEdit4("Color", g_chams.color,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    ImGui::Checkbox("Glow", &g_chams.glow);
    if (g_chams.glow) {
        ImGui::ColorEdit4("Glow color", g_chams.glowColor,
            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        ImGui::SliderFloat("Glow strength", &g_chams.glowStrength, 0.0f, 3.0f, "%.2f");
    }
    ImGui::Checkbox("Through walls", &g_chams.walls);
    ImGui::Separator();
    ImGui::Text("status: %s", thenwefuckin::chams::hooked() ? "active" : "idle");
    ImGui::Text("INSERT menu, END quit");
    ImGui::End();
}
} // namespace

int main() {
    if (!nc::g_process.Attach()) {
        MessageBoxA(NULL, "RobloxPlayerBeta.exe is not running.", "champoc", MB_ICONERROR);
        return 1;
    }
    nc::g_base = nc::g_process.base();
    nc::g_pid  = nc::g_process.pid();

    if (!OverlayInit()) {
        MessageBoxA(NULL, "overlay init failed", "champoc", MB_ICONERROR);
        return 1;
    }

    HWND roblox = FindWindowA(NULL, "Roblox");
    for (int i = 0; !roblox && i < 120; ++i) {
        roblox = FindWindowA("RobloxPlayerBeta", NULL);
        if (!roblox) Sleep(250);
    }

    bool running = true;
    DWORD lastRefresh = 0;
    while (running) {
        if (!OverlayBegin()) break;

        if (GetAsyncKeyState(VK_INSERT) & 1) {
            g_menuOpen = !g_menuOpen;
            SetClickThrough(!g_menuOpen);
        }
        if (GetAsyncKeyState(VK_END) & 1) running = false;

        if (roblox) MatchRobloxWindow(roblox);

        DWORD now = GetTickCount();
        if (now - lastRefresh >= 15) {
            lastRefresh = now;
            nc::RefreshPlayers();
        }

        DrawMenu();
        OverlayEnd();

        thenwefuckin::chams::tick();

        if (!nc::g_process.alive()) running = false;
        Sleep(1);
    }

    thenwefuckin::chams::stop();
    OverlayShutdown();
    return 0;
}
