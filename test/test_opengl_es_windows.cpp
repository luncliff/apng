/**
 * @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLSyncControlTest.cpp
 * @see https://github.com/google/angle/blob/master/util/windows/win32/Win32Window.cpp 
 */
#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>

#include "opengl.h"
#include <EGL/eglext_angle.h>

// DirectX 11
// clang-format off
#include <comdef.h>
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

using namespace std;
using namespace Microsoft::WRL;

auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger>;

TEST_CASE("eglQueryString", "[egl]") {
    auto stream = get_current_stream();
    //EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    SECTION("EGL_EXTENSIONS") {
        stream->info("EGL:");
        const auto txt = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
        REQUIRE(txt);
        const auto txtlen = strlen(txt);
        stream->info(" - EGL_EXTENSIONS:");
        auto offset = 0;
        for (auto i = 0u; i < txtlen; ++i) {
            if (isspace(txt[i]) == false)
                continue;
            const auto extension = string_view{txt + offset, i - offset};
            stream->info("   - {}", extension);
            offset = ++i;
        }
    }
}

TEST_CASE("without window/display", "[egl]") {
    auto stream = get_current_stream();

    SECTION("manual construction") {
        EGLNativeWindowType native_window{};
        EGLNativeDisplayType native_display = EGL_DEFAULT_DISPLAY;

        const EGLDisplay display = eglGetDisplay(native_display);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        EGLint major = 0, minor = 0;
        REQUIRE(eglInitialize(display, &major, &minor) == EGL_TRUE);
        REQUIRE(eglGetError() == EGL_SUCCESS);
        REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));

        EGLint count = 0;
        EGLConfig configs[10]{};
        eglChooseConfig(display, nullptr, configs, 10, &count);
        CAPTURE(count);

        const EGLContext context = eglCreateContext(display, configs[0], EGL_NO_CONTEXT, NULL);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        const EGLSurface surface_0 = EGL_NO_SURFACE; // read
        const EGLSurface surface_1 = EGL_NO_SURFACE; // draw
        eglMakeCurrent(display, surface_1, surface_0, context);
        REQUIRE(eglGetError() == EGL_SUCCESS);
    }

    SECTION("default display") {
        egl_helper_t egl{EGL_DEFAULT_DISPLAY};
        stream->info("OpenGL(EGL_DEFAULT_DISPLAY):");
        stream->info(" - GL_VERSION: {:s}", glGetString(GL_VERSION));
        stream->info(" - GL_VENDOR: {:s}", glGetString(GL_VENDOR));
        stream->info(" - GL_RENDERER: {:s}", glGetString(GL_RENDERER));
        stream->info(" - GL_SHADING_LANGUAGE_VERSION: {:s}", glGetString(GL_SHADING_LANGUAGE_VERSION));
        GLint count = 0;
        glGetIntegerv(GL_EXTENSIONS, &count);
        if (count > 0)
            stream->info(" - GL_EXTENSIONS:");
        for (auto i = 0; i < count; ++i)
            stream->info("   - {:s}", glGetStringi(GL_EXTENSIONS, i));
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
        egl_helper_t egl{window, is_console};
    }
}

struct dx11_device_t final {
    ComPtr<ID3D11Device> handle;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level;

  public:
    dx11_device_t() noexcept(false) : handle{}, context{}, level{} {
        if (auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, nullptr, 0, //
                                        D3D11_SDK_VERSION, handle.GetAddressOf(), &level, context.GetAddressOf());
            FAILED(hr))
            throw _com_error{hr};
    }
};
TEST_CASE("eglGetProcAddress == GetProcAddress", "[egl]") {
    auto hmodule = LoadLibraryW(L"libEGL");
    REQUIRE(hmodule);
    auto on_return = gsl::finally([hmodule]() { REQUIRE(FreeLibrary(hmodule)); });

    // both symbol must exist, but their address may different
    void* lhs = eglGetProcAddress("eglCreateDeviceANGLE");
    void* rhs = GetProcAddress(hmodule, "eglCreateDeviceANGLE");
    REQUIRE(lhs);
    REQUIRE(rhs);
    REQUIRE(lhs != rhs);

    lhs = eglGetProcAddress("eglReleaseDeviceANGLE");
    rhs = GetProcAddress(hmodule, "eglReleaseDeviceANGLE");
    REQUIRE(lhs);
    REQUIRE(rhs);
    REQUIRE(lhs != rhs);
}

TEST_CASE("Dx11 Device for ANGLE", "[egl]") {
    PFNEGLCREATEDEVICEANGLEPROC createDevice =
        reinterpret_cast<PFNEGLCREATEDEVICEANGLEPROC>(eglGetProcAddress("eglCreateDeviceANGLE"));
    REQUIRE(createDevice);
    PFNEGLRELEASEDEVICEANGLEPROC releaseDevice =
        reinterpret_cast<PFNEGLRELEASEDEVICEANGLEPROC>(eglGetProcAddress("eglReleaseDeviceANGLE"));
    REQUIRE(releaseDevice);

    dx11_device_t dx11_device{};
    EGLDeviceEXT device = createDevice(EGL_D3D11_DEVICE_ANGLE, dx11_device.handle.Get(), nullptr);
    REQUIRE(device != EGL_NO_DEVICE_EXT);
    REQUIRE(eglGetError() == EGL_SUCCESS);
    auto on_return = gsl::finally([releaseDevice, device]() { releaseDevice(device); });

    SECTION("EGLAttrib == ID3D11Device") {
        EGLAttrib attr{};
        eglQueryDeviceAttribEXT(device, EGL_D3D11_DEVICE_ANGLE, &attr);
        REQUIRE(eglGetError() == EGL_SUCCESS);

        ID3D11Device* handle = reinterpret_cast<ID3D11Device*>(attr);
        REQUIRE(handle);
        REQUIRE(dx11_device.level == handle->GetFeatureLevel());
    }
}

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
    explicit app_context_t(HINSTANCE happ, HWND hwnd, LPCSTR classname) : instance{happ}, window{hwnd}, winclass{} {
        winclass.lpszClassName = classname;
    }
    explicit app_context_t(HINSTANCE happ) : instance{happ}, window{}, winclass{} {
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
        this->window = CreateWindowEx(WS_EX_APPWINDOW, winclass.lpszClassName, winclass.lpszClassName,
                                      WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP, posX, posY, screenWidth,
                                      screenHeight, NULL, NULL, happ, NULL);

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
uintptr_t run(app_context_t& app, egl_helper_t& egl) noexcept(false) {
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        SleepEx(10, TRUE);

        glClearColor(37.0f / 255, 27.0f / 255, 82.0f / 255, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        // ...
        if (eglSwapBuffers(egl.display, egl.surface) == false)
            throw std::runtime_error{"eglSwapBuffers(mDisplay, mSurface"};
    }
    return msg.wParam;
}
