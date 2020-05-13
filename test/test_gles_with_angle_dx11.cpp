#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>
#include <string_view>

// GLFW for ES 2.0
// clang-format off
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2ext_angle.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
extern "C" {
#include "EGL/eglext_angle.h"
}
// clang-format on

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

struct egl_bundle_t {
    EGLNativeWindowType window{};
    EGLNativeDisplayType display = EGL_DEFAULT_DISPLAY;
    EGLDisplay mDisplay = EGL_NO_DISPLAY;
    EGLSurface mSurface = EGL_NO_SURFACE;
    EGLContext mContext = EGL_NO_CONTEXT;
    EGLConfig mConfig = 0;
    EGLint major, minor;

  public:
    egl_bundle_t() noexcept = default;
    explicit egl_bundle_t(EGLNativeDisplayType native) noexcept(false)
        : window{}, display{native} {
        mDisplay = eglGetDisplay(display);
        if (eglGetError() != EGL_SUCCESS) {
            throw std::runtime_error{"eglGetDisplay(EGL_DEFAULT_DISPLAY)"};
        }
        eglInitialize(mDisplay, &major, &minor);
        if (eglGetError() != EGL_SUCCESS) {
            throw std::runtime_error{"eglInitialize"};
        }
        EGLint count = 0;
        eglChooseConfig(mDisplay, nullptr, &mConfig, 1, &count);
        if (eglGetError() != EGL_SUCCESS) {
            throw std::runtime_error{"eglChooseConfig"};
        }
        if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
            throw std::runtime_error{"eglBindAPI(EGL_OPENGL_ES_API)"};
        }
        const EGLint attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        mContext = eglCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, attrs);
        if (eglGetError() != EGL_SUCCESS) {
            throw std::runtime_error{"eglCreateContext"};
        }
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, mContext);
        if (eglGetError() != EGL_SUCCESS) {
            throw std::runtime_error{"eglMakeCurrent"};
        }
    }
    egl_bundle_t(EGLNativeWindowType native, bool is_console) noexcept(false)
        : window{native}, display{GetDC(native)} {
        {
            const EGLint attrs[] = {
                EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE,
                EGL_DONT_CARE,
                EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE,
                EGL_DONT_CARE,
                EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
                EGL_NONE};
            mDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE,
                                                display, attrs);

            if (eglInitialize(mDisplay, &major, &minor) != EGL_TRUE)
                throw std::runtime_error{
                    "eglGetPlatformDisplayEXT, eglInitialize"};
        }
        {
            eglBindAPI(EGL_OPENGL_ES_API);
            if (eglGetError() != EGL_SUCCESS)
                throw std::runtime_error{"eglBindAPI(EGL_OPENGL_ES_API)"};
        }
        // Choose a config
        {
            const EGLint attrs[] = {EGL_NONE};
            EGLint num_attrs = 0;
            if (eglChooseConfig(mDisplay, attrs, &mConfig, 1, &num_attrs) !=
                EGL_TRUE)
                throw std::runtime_error{"eglChooseConfig"};
        }
        if (is_console == false) {
            const EGLint attrs[] = {EGL_DIRECT_COMPOSITION_ANGLE, EGL_TRUE,
                                    EGL_NONE};
            // Create window surface
            mSurface = eglCreateWindowSurface(mDisplay, mConfig, window, attrs);
            if (mSurface == nullptr)
                throw std::runtime_error{"eglCreateWindowSurface"};
        }
        // Create EGL context
        {
            const EGLint attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
            mContext =
                eglCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, attrs);
            if (eglGetError() != EGL_SUCCESS)
                throw std::runtime_error{"eglCreateContext"};
        }
        // Make the surface current
        {
            eglMakeCurrent(mDisplay, mSurface, mSurface, mContext);
            if (eglGetError() != EGL_SUCCESS)
                throw std::runtime_error{"eglMakeCurrent"};
        }
    }

    ~egl_bundle_t() {
        // Win32
        if (display)
            ReleaseDC(window, display);
        // EGL
        if (mSurface != EGL_NO_SURFACE)
            eglDestroySurface(mDisplay, mSurface);
        if (mContext != EGL_NO_CONTEXT)
            eglDestroyContext(mDisplay, mContext);
        if (mDisplay != EGL_NO_DISPLAY)
            eglTerminate(mDisplay);
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
        if (eglSwapBuffers(egl.mDisplay, egl.mSurface) == false)
            throw std::runtime_error{"eglSwapBuffers(mDisplay, mSurface"};
    }
    return msg.wParam;
}

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

TEST_CASE("eglQueryString", "[egl]") {
    // eglQueryString returns static, zero-terminated string
    SECTION("EGL_EXTENSIONS") {
        const auto txt = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
        REQUIRE(txt);
        const auto txtlen = strlen(txt);

        auto offset = 0;
        for (auto i = 0u; i < txtlen; ++i) {
            if (isspace(txt[i]) == false)
                continue;
            const auto extname = std::string_view{txt + offset, i - offset};
            CAPTURE(extname);
            offset = ++i;
        }
    }
}

// https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLSyncControlTest.cpp
// https://github.com/google/angle/blob/master/util/windows/win32/Win32Window.cpp
TEST_CASE("with window/display", "[dx11]") {
    app_context_t app{GetModuleHandle(NULL)};
    REQUIRE(app.instance);
    REQUIRE(app.window);

    SECTION("Game Loop") {
        constexpr bool is_console = false;
        egl_bundle_t gfx{app.window, is_console};
        REQUIRE(run(app, gfx, 60 * 4) == EXIT_SUCCESS);
    }
    SECTION("DirectX 11 Device + ANGLE") {
        dx11_context_t dx11{};
        EGLDeviceEXT device = eglCreateDeviceANGLE(EGL_D3D11_DEVICE_ANGLE, //
                                                   dx11.device, nullptr);
        REQUIRE(device != EGL_NO_DEVICE_EXT);
        eglReleaseDeviceANGLE(device);
    }
}

TEST_CASE("without window/display", "[egl]") {
    SECTION("manual construction") {
        egl_bundle_t egl{};
        egl.mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        REQUIRE(eglInitialize(egl.mDisplay, nullptr, nullptr) == EGL_TRUE);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        EGLint count = 0;
        eglChooseConfig(egl.mDisplay, nullptr, &egl.mConfig, 1, &count);
        CAPTURE(count);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));
        egl.mContext =
            eglCreateContext(egl.mDisplay, egl.mConfig, EGL_NO_CONTEXT, NULL);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        // ...

        eglMakeCurrent(egl.mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       egl.mContext);
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
