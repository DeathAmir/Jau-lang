#include "jgui_api.hpp"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include <d3d11.h>
#include <windows.h>

static HWND g_window = nullptr;
static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_context = nullptr;
static IDXGISwapChain* g_swap_chain = nullptr;
static ID3D11RenderTargetView* g_render_target = nullptr;
static bool g_class_registered = false;
static const char* g_class_name = "JauJGuiWindow";

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static void create_render_target() {
    ID3D11Texture2D* back_buffer = nullptr;
    if (g_swap_chain && SUCCEEDED(g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
        g_device->CreateRenderTargetView(back_buffer, nullptr, &g_render_target);
        back_buffer->Release();
    }
}

static void cleanup_render_target() {
    if (g_render_target) {
        g_render_target->Release();
        g_render_target = nullptr;
    }
}

static bool create_device(HWND window) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL feature_levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL selected{};
    const HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        feature_levels,
        2,
        D3D11_SDK_VERSION,
        &desc,
        &g_swap_chain,
        &g_device,
        &selected,
        &g_context
    );
    if (FAILED(result)) return false;
    create_render_target();
    return g_render_target != nullptr;
}

static void cleanup_device() {
    cleanup_render_target();
    if (g_swap_chain) { g_swap_chain->Release(); g_swap_chain = nullptr; }
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
}

static LRESULT WINAPI window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) return 1;
    switch (message) {
        case WM_SIZE:
            if (g_device && wparam != SIZE_MINIMIZED) {
                cleanup_render_target();
                if (g_swap_chain) {
                    g_swap_chain->ResizeBuffers(0, static_cast<UINT>(LOWORD(lparam)), static_cast<UINT>(HIWORD(lparam)), DXGI_FORMAT_UNKNOWN, 0);
                    create_render_target();
                }
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wparam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcA(window, message, wparam, lparam);
}

JGUI_API std::intptr_t jgui_create(const char* title, std::intptr_t width, std::intptr_t height) {
    if (g_window) return 1;

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = g_class_name;
    if (!RegisterClassExA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 0;
    g_class_registered = true;

    const int w = width > 0 ? static_cast<int>(width) : 960;
    const int h = height > 0 ? static_cast<int>(height) : 640;
    g_window = CreateWindowExA(0, g_class_name, title ? title : "JGui", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, w, h, nullptr, nullptr, wc.hInstance, nullptr);
    if (!g_window) return 0;
    if (!create_device(g_window)) {
        DestroyWindow(g_window);
        g_window = nullptr;
        cleanup_device();
        return 0;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(g_window);
    ImGui_ImplDX11_Init(g_device, g_context);

    ShowWindow(g_window, SW_SHOWDEFAULT);
    UpdateWindow(g_window);
    return 1;
}

JGUI_API std::intptr_t jgui_begin_frame() {
    if (!g_window || !ImGui::GetCurrentContext()) return 0;
    MSG message{};
    while (PeekMessageA(&message, nullptr, 0U, 0U, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
        if (message.message == WM_QUIT) return 0;
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    return 1;
}

JGUI_API std::intptr_t jgui_render() {
    if (!g_window || !ImGui::GetCurrentContext() || !g_context || !g_render_target || !g_swap_chain) return 0;
    ImGui::Render();
    const float clear_color[4] = {0.055f, 0.060f, 0.070f, 1.0f};
    g_context->OMSetRenderTargets(1, &g_render_target, nullptr);
    g_context->ClearRenderTargetView(g_render_target, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swap_chain->Present(1, 0);
    return 1;
}

JGUI_API std::intptr_t jgui_destroy() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    cleanup_device();
    if (g_window) {
        DestroyWindow(g_window);
        g_window = nullptr;
    }
    if (g_class_registered) {
        UnregisterClassA(g_class_name, GetModuleHandleA(nullptr));
        g_class_registered = false;
    }
    return 1;
}
