#include "overlay.hpp"
#include "overlay_class.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static IDCompositionDevice* g_pDCompDevice = nullptr;
static IDCompositionTarget* g_pDCompTarget = nullptr;
static IDCompositionVisual* g_pDCompVisual = nullptr;
static bool g_overlayIsChild = false;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == WM_DESTROY) { g_running = false; PostQuitMessage(0); return 0; }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

HWND MakeOverlay(HWND gameParent) {
    (void)gameParent;
    g_overlayIsChild = false;

    g_screenW = (float)GetSystemMetrics(SM_CXSCREEN);
    g_screenH = (float)GetSystemMetrics(SM_CYSCREEN);
    if (g_screenW < 1.f) g_screenW = 1.f;
    if (g_screenH < 1.f) g_screenH = 1.f;

    const char* cls = GetOverlayClassNameA();
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = cls;
    UnregisterClassA("DCompHostWindow", wc.hInstance);
    UnregisterClassA(cls, wc.hInstance);
    if (!RegisterClassExA(&wc))
        return nullptr;

    HWND h = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP |
        WS_EX_LAYERED | WS_EX_TRANSPARENT,
        cls, "", WS_POPUP,
        0, 0, (int)g_screenW, (int)g_screenH, nullptr, nullptr, wc.hInstance, nullptr);
    if (h)
        ShowWindow(h, SW_SHOW);
    return h;
}

void SyncOverlayToGame(HWND overlay, HWND game, bool menuOpen)
{
    if (!overlay || !game) return;
    (void)menuOpen;

    RECT r = {};
    GetClientRect(game, &r);
    g_screenW = (float)(r.right - r.left);
    g_screenH = (float)(r.bottom - r.top);
    if (g_screenW < 1.f) g_screenW = 1.f;
    if (g_screenH < 1.f) g_screenH = 1.f;

    if (g_overlayIsChild) {
        SetWindowPos(overlay, HWND_TOP, 0, 0,
            (int)g_screenW, (int)g_screenH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        return;
    }

    POINT pt = { 0, 0 };
    ClientToScreen(game, &pt);
    SetWindowPos(overlay, HWND_TOPMOST, pt.x, pt.y,
        (int)g_screenW, (int)g_screenH, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void CreateRT() {
    ID3D11Texture2D* pBB = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBB));
    g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_mainRT);
    pBB->Release();
}

bool InitD3D(HWND hWnd) {
    D3D_FEATURE_LEVEL fl;
    D3D_FEATURE_LEVEL flv[] = { D3D_FEATURE_LEVEL_11_0 };
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        flv,
        1,
        D3D11_SDK_VERSION,
        &g_pd3dDevice,
        &fl,
        &g_pd3dContext
    );
    if (FAILED(hr)) return false;

    IDXGIDevice* pDxgiDevice = nullptr;
    hr = g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pDxgiDevice));
    if (FAILED(hr)) return false;

    IDXGIAdapter* pDxgiAdapter = nullptr;
    hr = pDxgiDevice->GetAdapter(&pDxgiAdapter);
    if (FAILED(hr)) { pDxgiDevice->Release(); return false; }

    IDXGIFactory2* pDxgiFactory = nullptr;
    hr = pDxgiAdapter->GetParent(IID_PPV_ARGS(&pDxgiFactory));
    if (FAILED(hr)) { pDxgiDevice->Release(); pDxgiAdapter->Release(); return false; }

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = (UINT)g_screenW;
    sd.Height = (UINT)g_screenH;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Stereo = FALSE;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    sd.Flags = 0;

    IDXGISwapChain1* pSwapChain1 = nullptr;
    hr = pDxgiFactory->CreateSwapChainForComposition(pDxgiDevice, &sd, nullptr, &pSwapChain1);
    if (FAILED(hr)) {
        pDxgiDevice->Release();
        pDxgiAdapter->Release();
        pDxgiFactory->Release();
        return false;
    }
    g_pSwapChain = pSwapChain1;

    hr = DCompositionCreateDevice(pDxgiDevice, IID_PPV_ARGS(&g_pDCompDevice));
    if (FAILED(hr)) {
        pDxgiDevice->Release();
        pDxgiAdapter->Release();
        pDxgiFactory->Release();
        return false;
    }

    hr = g_pDCompDevice->CreateTargetForHwnd(hWnd, TRUE, &g_pDCompTarget);
    if (FAILED(hr)) {
        pDxgiDevice->Release();
        pDxgiAdapter->Release();
        pDxgiFactory->Release();
        return false;
    }

    hr = g_pDCompDevice->CreateVisual(&g_pDCompVisual);
    if (FAILED(hr)) {
        pDxgiDevice->Release();
        pDxgiAdapter->Release();
        pDxgiFactory->Release();
        return false;
    }

    hr = g_pDCompVisual->SetContent(g_pSwapChain);
    if (FAILED(hr)) {
        pDxgiDevice->Release();
        pDxgiAdapter->Release();
        pDxgiFactory->Release();
        return false;
    }

    hr = g_pDCompTarget->SetRoot(g_pDCompVisual);
    if (FAILED(hr)) {
        pDxgiDevice->Release();
        pDxgiAdapter->Release();
        pDxgiFactory->Release();
        return false;
    }

    hr = g_pDCompDevice->Commit();
    if (FAILED(hr)) {
        pDxgiDevice->Release();
        pDxgiAdapter->Release();
        pDxgiFactory->Release();
        return false;
    }

    pDxgiDevice->Release();
    pDxgiAdapter->Release();
    pDxgiFactory->Release();

    CreateRT();
    return true;
}

void DestroyOverlay() {
    if (g_hWnd) {
        DestroyWindow(g_hWnd);
        g_hWnd = nullptr;
    }
    UnregisterOverlayClassA();
}

void CleanupD3D() {
    if (g_pDCompVisual) { g_pDCompVisual->Release(); g_pDCompVisual = nullptr; }
    if (g_pDCompTarget) { g_pDCompTarget->Release(); g_pDCompTarget = nullptr; }
    if (g_pDCompDevice) { g_pDCompDevice->Release(); g_pDCompDevice = nullptr; }

    if (g_mainRT) { g_mainRT->Release(); g_mainRT = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}
