/**
 * @file test_opengl_es_windows.cpp
 *
 * @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLSyncControlTest.cpp
 * @see https://github.com/google/angle/blob/master/util/windows/win32/Win32Window.cpp 
 */

#include <catch2/catch.hpp>
#include <string_view>

#include "opengl_es.h"

// DirectX 11
// clang-format off
#include <wrl/client.h>

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <DirectXColors.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "comctl32")
// clang-format on

struct dx11_context_t {
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    D3D_FEATURE_LEVEL level;

  public:
    dx11_context_t() noexcept(false) : device{}, context{}, level{} {
        if (auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0,
                                        nullptr, 0, D3D11_SDK_VERSION, &device,
                                        &level, &context);
            FAILED(hr))
            throw std::runtime_error{"D3D11CreateDevice"};
    }
    ~dx11_context_t() noexcept {
        if (device)
            device->Release();
        if (context)
            context->Release();
    }
};

struct app_context_t final {
    HINSTANCE instance;
    HWND window;
    WNDCLASSEX winclass;

  public:
    static LRESULT CALLBACK on_key_msg(HWND hwnd, UINT umsg, //
                                       WPARAM wparam, LPARAM lparam) {
        switch (umsg) {
        case WM_KEYDOWN:
        case WM_KEYUP:
            return 0;
        default:
            return DefWindowProc(hwnd, umsg, wparam, lparam);
        }
    }

    static LRESULT CALLBACK on_wnd_msg(HWND hwnd, UINT umsg, //
                                       WPARAM wparam, LPARAM lparam) {
        switch (umsg) {
        case WM_DESTROY:
        case WM_CLOSE:
            PostQuitMessage(EXIT_SUCCESS); // == 0
            return 0;
        default:
            return on_key_msg(hwnd, umsg, wparam, lparam);
        }
    }

  public:
    explicit app_context_t(HINSTANCE happ, HWND hwnd, LPCSTR classname)
        : instance{happ}, window{hwnd}, winclass{} {
        winclass.lpszClassName = classname;
    }
    explicit app_context_t(HINSTANCE happ)
        : instance{happ}, window{}, winclass{} {
        // Setup the windows class with default settings.
        winclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        winclass.lpfnWndProc = on_wnd_msg;
        winclass.cbWndExtra = winclass.cbClsExtra = 0;
        winclass.hInstance = happ;
        winclass.hCursor = LoadCursor(NULL, IDC_ARROW);
        winclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        winclass.lpszMenuName = NULL;
        winclass.lpszClassName = TEXT("Engine");
        winclass.cbSize = sizeof(WNDCLASSEX);
        RegisterClassEx(&winclass);

        int posX = 0, posY = 0;
        auto screenWidth = 100 * 16, screenHeight = 100 * 9;
        // fullscreen: maximum size of the users desktop and 32bit
        if (true) {
            DEVMODE mode{};
            mode.dmSize = sizeof(mode);
            mode.dmPelsWidth = GetSystemMetrics(SM_CXSCREEN);
            mode.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
            mode.dmBitsPerPel = 32;
            mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            // Change the display settings to full screen.
            ChangeDisplaySettings(&mode, CDS_FULLSCREEN);
        }

        // Create the window with the screen settings and get the handle to it.
        this->window = CreateWindowEx(
            WS_EX_APPWINDOW, winclass.lpszClassName, winclass.lpszClassName,
            WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP, posX, posY,
            screenWidth, screenHeight, NULL, NULL, happ, NULL);

        // Bring the window up on the screen and set it as main focus.
        ShowWindow(this->window, SW_SHOW);
        SetForegroundWindow(this->window);
        SetFocus(this->window);
        ShowCursor(true);
    }
    ~app_context_t() {
        DestroyWindow(this->window);
        UnregisterClassA(this->winclass.lpszClassName, nullptr);
    }

    bool consume_message(MSG& msg) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return msg.message == WM_QUIT;
    }
};

// https://github.com/svenpilz/egl_offscreen_opengl/blob/master/egl_opengl_test.cpp
uint32_t run(app_context_t& app, //
             egl_bundle_t& egl, uint32_t repeat = INFINITE) noexcept(false) {
    MSG msg{};
    while ((app.consume_message(msg) == false) && repeat--) {
        // update
        SleepEx(10, TRUE);
        // clear/render
        glClearColor(37.0f / 255, 27.0f / 255, 82.0f / 255, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        // ...
        if (eglSwapBuffers(egl.display, egl.surface) == false)
            throw std::runtime_error{"eglSwapBuffers(mDisplay, mSurface"};
    }
    return msg.wParam;
}

TEST_CASE("without window/display", "[egl]") {
    SECTION("manual construction") {
        egl_bundle_t egl{};
        egl.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        REQUIRE(eglInitialize(egl.display, nullptr, nullptr) == EGL_TRUE);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        EGLint count = 0;
        eglChooseConfig(egl.display, nullptr, &egl.config, 1, &count);
        CAPTURE(count);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));
        egl.context = eglCreateContext(egl.display, egl.config, //
                                       EGL_NO_CONTEXT, NULL);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        // ...
        eglMakeCurrent(egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       egl.context);
        REQUIRE(eglGetError() == EGL_SUCCESS);
    }
    SECTION("default display") {
        egl_bundle_t egl{EGL_DEFAULT_DISPLAY};
    }
    SECTION("console window") {
        // https://support.microsoft.com/en-us/help/124103/how-to-obtain-a-console-window-handle-hwnd
        auto hwnd_from_console_title = []() {
            constexpr auto max_length = 800;
            char title[max_length]{};
            GetConsoleTitleA(title, max_length);
            return FindWindowA(NULL, title);
        };

        constexpr bool is_console = true;
        const HWND window = hwnd_from_console_title();
        REQUIRE(window);
        egl_bundle_t egl{window, is_console};
    }
}
