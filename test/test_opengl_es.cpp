/**
 * @see https://github.com/google/angle/blob/master/src/tests/egl_tests/EGLSyncControlTest.cpp
 * @see https://github.com/google/angle/blob/master/util/windows/win32/Win32Window.cpp 
 */
#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>

// clang-format off
#include <graphics.h>
#include <EGL/eglext_angle.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_EGL
#define GLFW_INCLUDE_ES3
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
// clang-format on

#include <pplawait.h>
#include <ppltasks.h>
#include <winrt/Windows.Foundation.h> // namespace winrt::Windows::Foundation
#include <winrt/Windows.System.h>     // namespace winrt::Windows::System

// clang-format off
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <DirectXTK/DirectXHelpers.h>
#include <DirectXTK/WICTextureLoader.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
// clang-format on

using namespace std;
using winrt::com_ptr;

TEST_CASE("eglGetProcAddress != GetProcAddress", "[egl]") {
    auto hmodule = LoadLibraryW(L"libEGL");
    REQUIRE(hmodule);
    auto on_return = gsl::finally([hmodule]() { REQUIRE(FreeLibrary(hmodule)); });
    SECTION("eglCreateDeviceANGLE") {
        // both symbol must exist, but their address may different
        void* lhs = eglGetProcAddress("eglCreateDeviceANGLE");
        void* rhs = GetProcAddress(hmodule, "eglCreateDeviceANGLE");
        REQUIRE(lhs);
        REQUIRE(rhs);
        REQUIRE(lhs != rhs);
    }
    SECTION("eglReleaseDeviceANGLE") {
        void* lhs = eglGetProcAddress("eglReleaseDeviceANGLE");
        void* rhs = GetProcAddress(hmodule, "eglReleaseDeviceANGLE");
        REQUIRE(lhs);
        REQUIRE(rhs);
        REQUIRE(lhs != rhs);
    }
}

TEST_CASE("GetProcAddress", "[opengl]") {
    auto hmodule = LoadLibraryW(L"libGLESv2");
    REQUIRE(hmodule);
    auto on_return = gsl::finally([hmodule]() { REQUIRE(FreeLibrary(hmodule)); });
    SECTION("glMapBufferRange") {
        void* symbol = GetProcAddress(hmodule, "glMapBufferRange");
        REQUIRE(symbol);
        REQUIRE(eglGetProcAddress("glMapBufferRange"));
    }
    SECTION("glUnmapBuffer") {
        void* symbol = GetProcAddress(hmodule, "glUnmapBuffer");
        REQUIRE(symbol);
    }
}

void get_extensions(EGLDisplay display, std::vector<std::string_view>& names) {
    if (const auto txt = eglQueryString(display /*EGL_NO_DISPLAY*/, EGL_EXTENSIONS)) {
        const auto txtlen = strlen(txt);
        auto offset = 0;
        for (auto i = 0u; i < txtlen; ++i) {
            if (isspace(txt[i]) == false)
                continue;
            names.emplace_back(txt + offset, i - offset);
            offset = ++i;
        }
    }
}

void print_info(EGLDisplay display) {
    spdlog::info("EGL:");
    if (gsl::czstring<> txt = eglQueryString(display, EGL_VERSION))
        spdlog::info("  EGL_VERSION: {}", txt);
    if (gsl::czstring<> txt = eglQueryString(display, EGL_VENDOR))
        spdlog::info("  EGL_VENDOR: '{}'", txt);
    if (gsl::czstring<> txt = eglQueryString(display, EGL_CLIENT_APIS))
        spdlog::info("  EGL_CLIENT_APIS: {}", txt);
    std::vector<std::string_view> extensions{};
    get_extensions(display, extensions);
    if (extensions.empty())
        return;
    spdlog::info("  EGL_EXTENSIONS:");
    for (auto name : extensions)
        spdlog::info("   - {}", name);
}

TEST_CASE("EGLContext setup/teardown", "[egl]") {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    REQUIRE(eglGetError() == EGL_SUCCESS);

    EGLint major = 0, minor = 0;
    REQUIRE(eglInitialize(display, &major, &minor) == EGL_TRUE);
    REQUIRE(eglGetError() == EGL_SUCCESS);
    auto on_return1 = gsl::finally([display]() {
        REQUIRE(eglTerminate(display)); // ...
    });
    print_info(display);
    REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));

    EGLint count = 0;
    EGLConfig configs[10]{};
    eglChooseConfig(display, nullptr, configs, 10, &count);
    REQUIRE(eglGetError() == EGL_SUCCESS);

    // OpenGL ES 3.0
    const EGLint attrs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    const EGLContext context = eglCreateContext(display, configs[0], EGL_NO_CONTEXT, attrs);
    REQUIRE(eglGetError() == EGL_SUCCESS);
    auto on_return2 = gsl::finally([display, context]() {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        REQUIRE(eglGetError() == EGL_SUCCESS);
        eglDestroyContext(display, context);
        REQUIRE(eglGetError() == EGL_SUCCESS);
    });
}

/// @see https://support.microsoft.com/en-us/help/124103/how-to-obtain-a-console-window-handle-hwnd
HWND get_hwnd_for_console() {
    constexpr auto max_length = 800;
    char title[max_length]{};
    GetConsoleTitleA(title, max_length);
    return FindWindowA(NULL, title);
}

TEST_CASE("console window handle", "[windows][!mayfail]") {
    HWND handle = get_hwnd_for_console();
    if (handle == NULL)
        FAIL("Faild to find HWND from console name");
}

TEST_CASE("EGLContext helper with Console", "[egl][!mayfail]") {
    auto handle = get_hwnd_for_console();
    if (handle == NULL)
        FAIL("Faild to find HWND from console name");
    context_t context{EGL_NO_CONTEXT};
    REQUIRE(context.is_valid());
    // we can't draw on console window.
    // for some reason ANGLE returns EGL_SUCCESS for this case ...
    REQUIRE(context.resume(handle) == EGL_BAD_NATIVE_WINDOW);
    REQUIRE(context.suspend() == EGL_SUCCESS);
}

auto create_opengl_window(gsl::czstring<> window_name) noexcept -> std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>;

auto start_opengl_test() -> gsl::final_action<void (*)()> {
    REQUIRE(glfwInit());
    return gsl::finally(&glfwTerminate);
}

TEST_CASE("GLFW + OpenGL ES", "[opengl][glfw]") {
    auto on_return = start_opengl_test();
    auto window = create_opengl_window("GLFW3");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());

    CAPTURE(glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    SECTION("poll event / swap buffer") {
        auto repeat = 120;
        while (glfwWindowShouldClose(window.get()) == false && repeat--) {
            glfwPollEvents();
            glClearColor(0, 0, static_cast<float>(repeat) / 255, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            // ...
            glfwSwapBuffers(window.get());
            glfwPollEvents();
        }
    }
}

TEST_CASE("EGLContext helper with GLFW", "[egl][glfw]") {
    auto on_return = start_opengl_test();
    auto window = create_opengl_window("GLFW3");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());
    REQUIRE(glfwGetWin32Window(window.get()));

    GIVEN("separate EGLContext") {
        context_t context{EGL_NO_CONTEXT};
        REQUIRE(context.is_valid());

        WHEN("create EGLSurface and MakeCurrent")
        THEN("EGL_BAD_ALLOC") {
            // window is already created for the context
            REQUIRE(context.resume(glfwGetWin32Window(window.get())) == EGL_BAD_ALLOC);
        }
    }
    GIVEN("shared EGLContext") {
        EGLContext shared_context = eglGetCurrentContext();
        REQUIRE(shared_context != EGL_NO_CONTEXT);

        context_t context{shared_context};
        REQUIRE(context.is_valid());

        WHEN("create EGLSurface and MakeCurrent")
        THEN("EGL_BAD_ALLOC") {
            // window is already created for the context
            REQUIRE(context.resume(glfwGetWin32Window(window.get())) == EGL_BAD_ALLOC);
        }
    }
}

/// @see https://docs.gl/
TEST_CASE("GLFW info", "[glfw]") {
    auto on_return = start_opengl_test();
    auto window = create_opengl_window("GLFW3");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());

    SECTION("GetString") {
        spdlog::info("OpenGL(EGL_DEFAULT_DISPLAY):");
        spdlog::info("  GL_VERSION: {:s}", glGetString(GL_VERSION));
        spdlog::info("  GL_VENDOR: {:s}", glGetString(GL_VENDOR));
        spdlog::info("  GL_RENDERER: {:s}", glGetString(GL_RENDERER));
        spdlog::info("  GL_SHADING_LANGUAGE_VERSION: {:s}", glGetString(GL_SHADING_LANGUAGE_VERSION));
        REQUIRE(glGetError() == GL_NO_ERROR);
    }
    SECTION("GetInteger") {
        // see https://www.khronos.org/registry/OpenGL/extensions/
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        if (count > 0)
            spdlog::info("  GL_EXTENSIONS:");
        for (auto i = 0; i < count; ++i)
            spdlog::info("   - {:s}", glGetStringi(GL_EXTENSIONS, i));

        GLint value = 0;
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &value);
        spdlog::info("  GL_MAX_UNIFORM_BLOCK_SIZE: {}", value);
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
        spdlog::info("  GL_MAX_TEXTURE_BUFFER_SIZE: {}", value);
        glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &value);
        spdlog::info("  GL_MAX_RENDERBUFFER_SIZE: {}", value);

        glGetIntegerv(GL_MAX_SAMPLES, &value);
        spdlog::info("  GL_MAX_SAMPLES: {}", value);
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &value);
        spdlog::info("  GL_MAX_COLOR_ATTACHMENTS: {}", value);
#if defined(GL_MAX_COMPUTE_WORK_GROUP_SIZE)
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_SIZE, &value);
        spdlog::info("  GL_MAX_COMPUTE_WORK_GROUP_SIZE: {}", value);
#endif
        CAPTURE(glGetError());
    }
}

struct app_context_t final {
    HINSTANCE instance;
    HWND window;
    WNDCLASSEXW winclass;
    DEVMODEW mode;

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
    explicit app_context_t(HINSTANCE happ, LPCWSTR class_name = L"----")
        : instance{happ}, window{}, winclass{}, mode{} {
        // Setup the windows class with default settings.
        winclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        winclass.lpfnWndProc = on_wnd_msg;
        winclass.cbWndExtra = winclass.cbClsExtra = 0;
        winclass.hInstance = happ;
        winclass.hCursor = LoadCursor(NULL, IDC_ARROW);
        winclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        winclass.lpszMenuName = NULL;
        winclass.lpszClassName = class_name;
        winclass.cbSize = sizeof(WNDCLASSEXW);
        RegisterClassExW(&winclass);

        int posX = 0, posY = 0;
        auto screenWidth = 100 * 16, screenHeight = 100 * 9;
        // fullscreen: maximum size of the users desktop and 32bit
        if (true) {
            mode.dmSize = sizeof(mode);
            mode.dmPelsWidth = GetSystemMetrics(SM_CXSCREEN);
            mode.dmPelsHeight = GetSystemMetrics(SM_CYSCREEN);
            mode.dmBitsPerPel = 32;
            mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            // Change the display settings to full screen.
            ChangeDisplaySettingsW(&mode, CDS_FULLSCREEN);
        }

        // Create the window with the screen settings and get the handle to it.
        this->window = CreateWindowExW(WS_EX_APPWINDOW, winclass.lpszClassName, winclass.lpszClassName,
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
        UnregisterClassW(this->winclass.lpszClassName, nullptr);
    }

    bool consume_message(MSG& msg) {
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return msg.message != WM_QUIT;
    }

    // https://github.com/svenpilz/egl_offscreen_opengl/blob/master/egl_opengl_test.cpp
    uintptr_t run(app_context_t& app, EGLDisplay display, EGLSurface surface) noexcept(false) {
        MSG msg{};
        while (consume_message(msg)) {
            SleepEx(10, TRUE);
            glClearColor(37.0f / 255, 27.0f / 255, 82.0f / 255, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            // ...
            if (eglSwapBuffers(display, surface) == false)
                throw std::runtime_error{"eglSwapBuffers(mDisplay, mSurface"};
        }
        return msg.wParam;
    }
};

TEST_CASE("EGLContext helper with Win32 HINSTANCE", "[egl][windows]") {
    app_context_t app{GetModuleHandleW(NULL)};
    REQUIRE(app.instance);
    REQUIRE(app.window);
    REQUIRE(eglGetCurrentContext() == EGL_NO_CONTEXT);

    context_t context{EGL_NO_CONTEXT};
    REQUIRE(context.is_valid());
    REQUIRE(context.resume(app.window) == EGL_SUCCESS);

    SECTION("GetString") {
        spdlog::info("OpenGL(EGL_DEFAULT_DISPLAY):");
        spdlog::info("  GL_VERSION: {:s}", glGetString(GL_VERSION));
        spdlog::info("  GL_VENDOR: {:s}", glGetString(GL_VENDOR));
        spdlog::info("  GL_RENDERER: {:s}", glGetString(GL_RENDERER));
        spdlog::info("  GL_SHADING_LANGUAGE_VERSION: {:s}", glGetString(GL_SHADING_LANGUAGE_VERSION));
        REQUIRE(glGetError() == GL_NO_ERROR);
    }
}

// see https://www.roxlu.com/2014/048/fast-pixel-transfers-with-pixel-buffer-objects
TEST_CASE("PixelBufferObject", "[opengl][glfw]") {
    auto on_return = start_opengl_test();
    auto window = create_opengl_window("GLFW3");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());
    GLint w = 0, h = 0;
    glfwGetFramebufferSize(window.get(), &w, &h);
    glViewport(0, 0, w, h);
    REQUIRE(glGetError() == GL_NO_ERROR);

    SECTION("pixel buffer unpack") {
        GLuint pbos[2]{}; // 0 - GL_STREAM_READ, 1 - GL_STREAM_DRAW
        glGenBuffers(2, pbos);
        REQUIRE(glGetError() == GL_NO_ERROR);
        auto on_return = gsl::finally([pbos]() {
            glDeleteBuffers(2, pbos);
            REQUIRE(glGetError() == GL_NO_ERROR);
        });
        // 1 is for write (upload)
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[1]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, w * h * 4, nullptr, GL_STREAM_DRAW);
        REQUIRE(glGetError() == GL_NO_ERROR);
        void* mapping1 = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, w * h * 4, //
                                          GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
        REQUIRE(glGetError() == GL_NO_ERROR);
        REQUIRE(mapping1);
        REQUIRE(glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER));
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    SECTION("pixel buffer pack") {
        GLuint pbos[2]{};
        glGenBuffers(2, pbos);
        REQUIRE(glGetError() == GL_NO_ERROR);
        auto on_return = gsl::finally([pbos]() {
            glDeleteBuffers(2, pbos);
            REQUIRE(glGetError() == GL_NO_ERROR);
        });

        for (auto i : {0, 1}) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, w * h * 4, nullptr, GL_STREAM_READ);
            REQUIRE(glGetError() == GL_NO_ERROR);
            REQUIRE(glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, w * h * 4, GL_MAP_READ_BIT));
            REQUIRE(glUnmapBuffer(GL_PIXEL_PACK_BUFFER)); // must be unmapped before glReadPixels
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            REQUIRE(glGetError() == GL_NO_ERROR);
        }

        // read from the latest draw result
        glReadBuffer(GL_BACK);
        REQUIRE(glGetError() == GL_NO_ERROR);

        // render + present
        for (auto i : {0, 1}) {
            glClearColor(0, static_cast<float>(i), 1, 1); // 0 --BA, 1 -GBA
            glClear(GL_COLOR_BUFFER_BIT);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[i]);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, 0); // fbo -> pbo
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            REQUIRE(glGetError() == GL_NO_ERROR);
            glfwSwapBuffers(window.get());
        }

        // create mapping and check the pixel value
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[0]);
        if (const void* ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, w * h * 4, //
                                               GL_MAP_READ_BIT)) {
            // notice glClearColor 0.0f == 255 (black)
            const uint32_t blue = 0xFF'FF'00'00;
            uint32_t value = *reinterpret_cast<const uint32_t*>(ptr);
            REQUIRE(glUnmapBuffer(GL_PIXEL_PACK_BUFFER));
            REQUIRE(value == blue);
        } else {
            FAIL(glGetError());
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[1]);
        if (const void* ptr = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, w * h * 4, //
                                               GL_MAP_READ_BIT)) {
            // notice glClearColor 0.0f == 255 (black)
            const uint32_t blue_green = 0xFF'FF'FF'00;
            uint32_t value = *reinterpret_cast<const uint32_t*>(ptr);
            REQUIRE(glUnmapBuffer(GL_PIXEL_PACK_BUFFER));
            REQUIRE(value == blue_green);
        } else {
            FAIL(glGetError());
        }
    }
}

#if FALSE
TEST_CASE("OpenGL readback_t", "[opengl][glfw]") {
    auto stream = get_current_stream();
    auto on_return = start_opengl_test();
    auto window = create_opengl_window("GLFW3");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());
    GLint frame[4]{};
    glGetIntegerv(GL_VIEWPORT, frame);
    REQUIRE(glGetError() == GL_NO_ERROR);
    REQUIRE(frame[2] * frame[3] > 0);

    readback_callback_t is_untouched = [](void*, const void* mapping, size_t) {
        REQUIRE(*reinterpret_cast<const uint32_t*>(mapping) == 0x00'00'00'00); // ABGR in 32 bpp
    };
    readback_callback_t is_blue = [](void*, const void* mapping, size_t) {
        REQUIRE(*reinterpret_cast<const uint32_t*>(mapping) == 0xFF'FF'00'00);
    };
    readback_callback_t is_blue_green = [](void*, const void* mapping, size_t) {
        REQUIRE(*reinterpret_cast<const uint32_t*>(mapping) == 0xFF'FF'FF'00);
    };
    opengl_readback_t readback{frame[2], frame[3]};
    glReadBuffer(GL_BACK);
    REQUIRE(glGetError() == GL_NO_ERROR);

    uint16_t count = 20;
    while (--count) {
        auto const front = static_cast<uint16_t>((count + 1) % 2);
        auto const back = static_cast<uint16_t>(count % 2);
        // perform rendering
        glClearColor(0, static_cast<float>(back), 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        // pack the back buffer
        if (auto ec = readback.pack(back, 0, frame))
            FAIL(ec);

        // map the front buffer and read
        if (count == 19) {
            // if there was no read, the pixel buffer object is clean and must hold zero (0,0,0,0)
            if (auto ec = readback.map_and_invoke(front, is_untouched, nullptr))
                FAIL(ec);
        } else {
            // if back == 0, the clear color of previous frame is (0,1,1,1). this is blue_green.
            // if `back` index equals 1, the color is (0,0,1,1), which is blue.
            if (auto ec = readback.map_and_invoke(front, back == 0 ? is_blue_green : is_blue, nullptr))
                FAIL(ec);
        }
        glfwSwapBuffers(window.get());
    }
}
#endif

/**
 * @see https://www.glfw.org/docs/latest/window_guide.html#GLFW_CONTEXT_CREATION_API_hint
 */
auto create_opengl_window(gsl::czstring<> window_name) noexcept -> std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> {
#if defined(__APPLE__)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API); // OpenGL
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // 3.2 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
#elif defined(_WIN32)
#if defined(GLFW_INCLUDE_ES2) || defined(GLFW_INCLUDE_ES3)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API); // OpenGL ES
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#if defined(GLFW_INCLUDE_ES2)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2); // ES 2.0
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#elif defined(GLFW_INCLUDE_ES3)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // ES 3.0+
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // ANGLE supports EGL_KHR_debug
#endif
#endif // defined(GLFW_INCLUDE_ES2) || defined(GLFW_INCLUDE_ES3)
#endif // __APPLE__ || _WIN32
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    const auto axis = 160 * 5;
    return std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>{glfwCreateWindow(axis, axis, window_name, NULL, NULL),
                                                              &glfwDestroyWindow};
}
