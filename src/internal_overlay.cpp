#include "sdk.hpp"
#include "memory.hpp"
#include "utils.hpp"
#include "offsets.hpp"
#include "overlay.hpp"
#include "render.hpp"
#include "features.hpp"
#include "freecam.h"
#include "thirdperson.h"
#include "outline.h"
#include "config.h"
#include "heroicons.h"
#include "abilityicons.h"
#include "rs_menu.h"
#include "menu_style.h"
#include "overlay_class.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

static HMODULE g_dllModule = nullptr;
static OverwatchSDK g_internalSdk;
static volatile bool g_overlayRunning = false;
static volatile LONG g_overlayStarted = 0;
static DWORD g_overlayThreadId = 0;

static void ReleaseIconTextures()
{
    for (auto& kv : g_heroIcons)
        if (kv.second) kv.second->Release();
    g_heroIcons.clear();
    g_heroIconAspect.clear();
    for (auto& kv : g_abilityIcons)
        if (kv.second) kv.second->Release();
    g_abilityIcons.clear();
}

static void ToggleOverlayMenu(HWND gameHwnd)
{
    g_menuOpen = !g_menuOpen;
    LONG ex = GetWindowLongA(g_hWnd, GWL_EXSTYLE);
    if (g_menuOpen) {
        ex &= ~WS_EX_TRANSPARENT;
        SetWindowLongA(g_hWnd, GWL_EXSTYLE, ex);
        while (ShowCursor(TRUE) < 0);
    } else {
        ex |= WS_EX_TRANSPARENT;
        SetWindowLongA(g_hWnd, GWL_EXSTYLE, ex);
        while (ShowCursor(FALSE) >= 0);
    }
    if (gameHwnd)
        SyncOverlayToGame(g_hWnd, gameHwnd, g_menuOpen);
}

static bool MenuKeyPressed(int vk)
{
    static bool wasDown[256] = {};
    BYTE keys[256] = {};
    if (!GetKeyboardState(keys)) return false;
    bool down = (keys[vk] & 0x80) != 0;
    bool pressed = down && !wasDown[vk];
    wasDown[vk] = down;
    return pressed;
}

static void StartOverlayViaPool();

static DWORD WINAPI DelayedFreeLibrary(LPVOID param)
{
    Sleep(300);
    FreeLibraryAndExitThread((HMODULE)param, 0);
    return 0;
}

static void InternalOverlayMain()
{
    SDK = &g_internalSdk;
    g_internalSdk.hProcess = GetCurrentProcess();
    g_internalSdk.dwGameBase = (uint64_t)GetModuleHandleW(nullptr);

    timeBeginPeriod(1);

    HWND gameHwnd = nullptr;
    while (!gameHwnd) {
        gameHwnd = FindWindowA(S("TankWindowClass"), nullptr);
        if (!gameHwnd) Sleep(200);
    }

    ReadEntities();
    ReadViewMatrix();

    g_hWnd = MakeOverlay(gameHwnd);
    if (!g_hWnd || !InitD3D(g_hWnd)) {
        CleanupD3D();
        DestroyOverlay();
        timeEndPeriod(1);
        g_overlayRunning = false;
        return;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 3;
    font_cfg.OversampleV = 3;
    g_defaultFont = io.Fonts->AddFontDefault(&font_cfg);
    g_menuFont = io.Fonts->AddFontFromFileTTF(S("C:\\Windows\\Fonts\\tahoma.ttf"), 14.f, &font_cfg);
    if (!g_menuFont) g_menuFont = g_defaultFont;
    io.Fonts->Build();
    InitRsMenuStyle();
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
    InitHeroIcons();
    InitAbilityIcons();
    CfgLd();
    if (!g_draw3dBox && !g_drawBoxes && !g_drawSkeleton && !g_drawLines)
        g_draw3dBox = true;

    RegisterHotKey(nullptr, 1, MOD_NOREPEAT, VK_INSERT);

    LARGE_INTEGER freq = {}, lastSave = {};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastSave);
    const double freqMs = freq.QuadPart / 1000.0;

    MSG msg = {};
    int noGameFrames = 0;

    while (g_running) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY && msg.wParam == 1) {
                ToggleOverlayMenu(gameHwnd);
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!g_running) break;

        LARGE_INTEGER qpcNow;
        QueryPerformanceCounter(&qpcNow);

        if (!gameHwnd || !IsWindow(gameHwnd))
            gameHwnd = FindWindowA(S("TankWindowClass"), nullptr);

        if (!gameHwnd) {
            if (g_hWnd)
                ShowWindow(g_hWnd, SW_HIDE);
            noGameFrames++;
            if (noGameFrames > 60) {
                g_running = false;
                break;
            }
            Sleep(16);
            continue;
        }
        noGameFrames = 0;
        SyncOverlayToGame(g_hWnd, gameHwnd, g_menuOpen);

        if (MenuKeyPressed(VK_INSERT))
            ToggleOverlayMenu(gameHwnd);

        if ((double)(qpcNow.QuadPart - lastSave.QuadPart) / freqMs >= 5000.0) {
            CfgSav();
            lastSave = qpcNow;
        }

        ReadEntities();
        ReadViewMatrix();
        if (g_freecam) ApplyFreecam();
        else if (g_thirdPerson) ApplyThirdPerson();
        DoLocalPlayerDetection();
        UpdateGameWorldInfo();
        UpdateScreenPositions();
        UpdateESPLogic();

        {
            static bool s_glowWasOn = false;
            if (g_glow) {
                static uint64_t s_lastGlow = 0;
                uint64_t nowMs = GetTickCount64();
                if (nowMs - s_lastGlow >= 120) {
                    s_lastGlow = nowMs;
                    ApplyEngineGlow();
                }
                s_glowWasOn = true;
            } else if (s_glowWasOn) {
                s_glowWasOn = false;
                ClearEngineGlow();
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (g_menuOpen) RenderMenu();
        DrawESP();
        ImGui::Render();

        g_pd3dContext->OMSetRenderTargets(1, &g_mainRT, nullptr);
        float clr[4] = { 0, 0, 0, 0 };
        g_pd3dContext->ClearRenderTargetView(g_mainRT, clr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(0, 0);

        if (!g_menuOpen) Sleep(0);
        else Sleep(1);
    }

    CfgSav();

    if (g_glow) {
        g_glow = false;
        ClearEngineGlow();
    }

    if (g_menuOpen) {
        g_menuOpen = false;
        while (ShowCursor(TRUE) < 0);
    }

    if (g_hWnd)
        ShowWindow(g_hWnd, SW_HIDE);

    UnregisterHotKey(nullptr, 1);

    timeEndPeriod(1);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    ReleaseIconTextures();
    CleanupD3D();
    DestroyOverlay();

    MSG drain;
    while (PeekMessageA(&drain, nullptr, 0, 0, PM_REMOVE)) {}

    g_overlayRunning = false;

    if (g_requestUnload && g_dllModule) {
        HMODULE h = g_dllModule;
        g_dllModule = nullptr;
        g_overlayModule = nullptr;
        InterlockedExchange(&g_overlayStarted, 0);
        HANDLE t = CreateThread(nullptr, 0, DelayedFreeLibrary, h, 0, nullptr);
        if (t) CloseHandle(t);
    }
}

static VOID CALLBACK OverlayPoolWork(PTP_CALLBACK_INSTANCE, PVOID, PTP_WORK work)
{
    CloseThreadpoolWork(work);
    g_overlayThreadId = GetCurrentThreadId();
    g_overlayRunning = true;
    InternalOverlayMain();
}

static void StartOverlayViaPool()
{
    if (g_overlayRunning) return;
    PTP_WORK work = CreateThreadpoolWork(OverlayPoolWork, nullptr, nullptr);
    if (!work) return;
    SubmitThreadpoolWork(work);
}

extern "C" __declspec(dllexport) void blind_restart()
{
    if (g_overlayRunning) return;
    g_running = true;
    g_requestUnload = false;
    g_menuOpen = false;
    InterlockedExchange(&g_overlayStarted, 1);
    StartOverlayViaPool();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        g_dllModule = hModule;
        g_overlayModule = hModule;
        g_running = true;
        g_requestUnload = false;
        if (InterlockedCompareExchange(&g_overlayStarted, 1, 0) == 0)
            StartOverlayViaPool();
        break;
    case DLL_PROCESS_DETACH:
        g_running = false;
        InterlockedExchange(&g_overlayStarted, 0);
        if (GetCurrentThreadId() != g_overlayThreadId) {
            for (int i = 0; i < 500 && g_overlayRunning; ++i)
                Sleep(10);
        }
        break;
    }
    return TRUE;
}
