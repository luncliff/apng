/**
 * @author Park DongHa (luncliff@gmail.com)
 * @see https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglIntro.xhtml
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
    EGLDisplay es_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;
    REQUIRE(eglInitialize(es_display, &major, &minor));
    auto on_return_1 = gsl::finally([es_display]() { REQUIRE(eglTerminate(es_display)); });

    REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));
    print_info(es_display);

    EGLint count = 0;
    EGLConfig es_configs[10]{};
    eglChooseConfig(es_display, nullptr, es_configs, 10, &count);
    REQUIRE(eglGetError() == EGL_SUCCESS);

    // Request OpenGL ES 3.x Debug mode
    EGLint attrs[]{EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE, EGL_NONE};
    EGLContext es_context = eglCreateContext(es_display, es_configs[0], EGL_NO_CONTEXT, attrs);
    if (es_context == EGL_NO_CONTEXT)
        FAIL(eglGetError());
    REQUIRE(eglMakeCurrent(es_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    eglDestroyContext(es_display, es_context);
    REQUIRE(eglGetError() == EGL_SUCCESS);
}

TEST_CASE("GL_FRAMEBUFFER_UNDEFINED", "[egl]") {
    EGLDisplay es_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint es_minor = 0;
    REQUIRE(eglInitialize(es_display, nullptr, &es_minor));
    auto on_return_1 = gsl::finally([es_display]() { REQUIRE(eglTerminate(es_display)); });

    if (es_minor < 5) {
        WARN("Need EGL 1.5+", eglQueryString(es_display, EGL_VERSION));
        return;
    }

    EGLint count = 0;
    EGLConfig es_configs[3]{};
    REQUIRE(eglChooseConfig(es_display, nullptr, es_configs, 3, &count));

    // Request OpenGL ES 3.x Debug mode
    EGLint attrs[]{EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE, EGL_NONE};
    EGLContext es_context = eglCreateContext(es_display, es_configs[0], EGL_NO_CONTEXT, attrs);
    if (es_context == EGL_NO_CONTEXT)
        FAIL(eglGetError());

    if (eglMakeCurrent(es_display, EGL_NO_SURFACE, EGL_NO_SURFACE, es_context) == false)
        FAIL(eglGetError());
    REQUIRE(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_UNDEFINED);
    REQUIRE(eglMakeCurrent(es_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
}

TEST_CASE("EGL_EXTENSIONS", "[egl][!mayfail]") {
    EGLDisplay es_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint versions[2]{};
    REQUIRE(eglInitialize(es_display, versions + 0, versions + 1) == EGL_TRUE);
    REQUIRE(eglGetError() == EGL_SUCCESS);
    auto on_return_1 = gsl::finally([es_display]() { REQUIRE(eglTerminate(es_display)); });

    REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));
    SECTION("KHR") {
        CHECK(has_extension(es_display, "EGL_KHR_gl_texture_2D_image"));
        // https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_fence_sync.txt
        CHECK(has_extension(es_display, "EGL_KHR_fence_sync"));
        // https://www.khronos.org/registry/EGL/extensions/KHR/EGL_KHR_wait_sync.txt
        CHECK(has_extension(es_display, "EGL_KHR_wait_sync"));
    }
    SECTION("ANGLE") {
        CHECK(has_extension(es_display, "EGL_ANGLE_d3d_share_handle_client_buffer"));
        CHECK(has_extension(es_display, "EGL_ANGLE_surface_d3d_texture_2d_share_handle"));
    }
}

TEST_CASE("EGL_KHR_fence_sync/EGL_KHR_wait_sync", "[egl][!mayfail]") {
    EGLDisplay es_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    egl_context_t context{es_display, EGL_NO_CONTEXT};
    REQUIRE(context.is_valid());
    REQUIRE(context.suspend() == EGL_SUCCESS); // the context is NOT bound

    REQUIRE(es_display == eglGetCurrentDisplay());
    // some version may not support these extensions...
    REQUIRE(has_extension(es_display, "EGL_KHR_fence_sync"));
    REQUIRE(has_extension(es_display, "EGL_KHR_wait_sync"));

    SECTION("EGL_SYNC_FENCE") {
        EGLSync fence = eglCreateSync(es_display, EGL_SYNC_FENCE, nullptr);
        REQUIRE(fence);
        REQUIRE(eglDestroySync(es_display, fence));
    }
}

void setup_egl_config(EGLDisplay display, EGLConfig& config, EGLint& minor) {
    EGLint versions[2]{};
    REQUIRE(eglInitialize(display, versions + 0, versions + 1));
    minor = versions[1];

    EGLint count = 0;
    EGLint attrs[]{EGL_SURFACE_TYPE,
                   EGL_PBUFFER_BIT, // 1
                   EGL_RENDERABLE_TYPE,
                   EGL_OPENGL_ES2_BIT, // 3
                   EGL_DEPTH_SIZE,
                   16, // 5
                   EGL_BLUE_SIZE,
                   8,
                   EGL_GREEN_SIZE,
                   8,
                   EGL_RED_SIZE,
                   8,
                   EGL_ALPHA_SIZE,
                   8,
                   EGL_NONE};
    // for 1.5+
    if (minor >= 5) {
        attrs[3] |= EGL_OPENGL_ES3_BIT;
        // ...
    }
    if (eglChooseConfig(display, attrs, &config, 1, &count) == false)
        FAIL(eglGetError());
}

TEST_CASE("PixelBuffer Surface", "[egl]") {
    EGLDisplay es_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLConfig es_config = EGL_NO_CONFIG_KHR;
    EGLint es_minor = 0;
    setup_egl_config(es_display, es_config, es_minor);
    auto on_return_1 = gsl::finally([es_display]() { REQUIRE(eglTerminate(es_display)); });

    // see https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglCreatePbufferSurface.xhtml
    // EGL_GL_COLORSPACE requires 1.5+. Default is EGL_GL_COLORSPACE_SRGB // EGL_GL_COLORSPACE_LINEAR?
    SECTION("RGB24(Simple)") {
        EGLint attrs[]{// EGL_TEXTURE_TARGET,
                       // EGL_TEXTURE_2D,
                       // EGL_TEXTURE_FORMAT,
                       // EGL_TEXTURE_RGB,
                       // EGL_MIPMAP_TEXTURE,
                       // EGL_FALSE,
                       EGL_WIDTH, 512, EGL_HEIGHT, 512, EGL_NONE};
        EGLSurface surface = eglCreatePbufferSurface(es_display, es_config, attrs);
        if (surface == EGL_NO_SURFACE) {
            auto ec = eglGetError();
            FAIL(ec);
        }
        if (eglDestroySurface(es_display, surface) == false)
            FAIL(eglGetError());
    }
    SECTION("RGBA32(Simple)") {
        EGLint attrs[]{EGL_WIDTH, 512, EGL_HEIGHT, 512, EGL_NONE};
        EGLSurface surface = eglCreatePbufferSurface(es_display, es_config, attrs);
        if (surface == EGL_NO_SURFACE)
            FAIL(eglGetError());
        if (eglDestroySurface(es_display, surface) == false)
            FAIL(eglGetError());
    }
}

TEST_CASE("EGLContext - PixelBuffer Surface", "[egl]") {
    EGLDisplay es_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    egl_context_t context{es_display, EGL_NO_CONTEXT};
    REQUIRE(context.is_valid());

    EGLint count = 1;
    EGLConfig es_configs[1];
    REQUIRE(context.get_configs(es_configs, count, nullptr) == 0);

    EGLint attrs[]{EGL_WIDTH, 1024, EGL_HEIGHT, 512, EGL_NONE};
    EGLSurface es_surface = eglCreatePbufferSurface(es_display, es_configs[0], attrs);
    REQUIRE(eglGetError() == EGL_SUCCESS);

    REQUIRE(context.resume(es_surface, es_configs[0]) == EGL_SUCCESS);
    REQUIRE(context.surface_width == 1024);
    REQUIRE(context.surface_height == 512);
    REQUIRE(eglGetCurrentSurface(EGL_READ) == es_surface);
    REQUIRE(eglGetCurrentSurface(EGL_DRAW) == es_surface);

    context.terminate();
    REQUIRE(eglTerminate(es_display));
}

bool has_extension(std::string_view name) noexcept {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    if (auto ec = glGetError(); ec != GL_NO_ERROR)
        return false;
    for (auto i = 0; i < count; ++i) {
        const auto extension = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (extension == name)
            return true;
    }
    return false;
}

/// @see https://www.khronos.org/opengl/wiki/Synchronization
/// @see http://docs.gl/es3/glFenceSync
TEST_CASE("OpenGL Sync - Fence", "[opengl][synchronization]") {
    EGLDisplay es_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    egl_context_t context{es_display, EGL_NO_CONTEXT};
    REQUIRE(context.is_valid());
    auto on_return = gsl::finally([es_display]() { eglTerminate(es_display); });

    EGLConfig es_configs[1]{EGL_NO_CONFIG_KHR};
    EGLSurface es_surface = EGL_NO_SURFACE;
    {
        EGLint count = 1;
        REQUIRE(context.get_configs(es_configs, count, nullptr) == 0);
        EGLint attrs[]{EGL_WIDTH, 128, EGL_HEIGHT, 128, EGL_NONE};
        es_surface = eglCreatePbufferSurface(es_display, es_configs[0], attrs);
        REQUIRE(eglGetError() == EGL_SUCCESS);
    }
    REQUIRE(context.resume(es_surface, es_configs[0]) == EGL_SUCCESS);
    REQUIRE(glGetString(GL_VERSION));

    SECTION("GL_SYNC_GPU_COMMANDS_COMPLETE") {
        GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        REQUIRE(glIsSync(fence));
        auto on_return = gsl::finally([fence]() { glDeleteSync(fence); });

        /// @see http://docs.gl/es3/glGetSynciv
        SECTION("Get") {
            GLsizei count{};
            GLint values[1]{}; // the following properties use single value
            glGetSynciv(fence, GL_OBJECT_TYPE, 1, &count, values);
            REQUIRE(count == 1);
            REQUIRE(values[0] == GL_SYNC_FENCE);
            glGetSynciv(fence, GL_SYNC_CONDITION, 1, &count, values);
            REQUIRE(count == 1);
            REQUIRE(values[0] == GL_SYNC_GPU_COMMANDS_COMPLETE);

            glGetSynciv(fence, GL_SYNC_STATUS, 1, &count, values);
            REQUIRE(count == 1);
            REQUIRE(values[0] == GL_UNSIGNALED);

            glGetSynciv(fence, GL_SYNC_FLAGS, 1, &count, values);
            REQUIRE(count == 1);
        }
        /// @see http://docs.gl/es3/glClientWaitSync
        SECTION("Wait without signal") {
            GLuint64 nano = 50'000;
            GLenum result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT | 0, nano);
            REQUIRE(result == GL_TIMEOUT_EXPIRED);
        }
        SECTION("Wait after Flush") {
            // make some commands and flush
            glClear(GL_COLOR_BUFFER_BIT);
            glFlush();
            REQUIRE(glGetError() == GL_NO_ERROR);
            // there are some expected values...
            switch (GLenum result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT | 0, GL_TIMEOUT_IGNORED)) {
            case GL_ALREADY_SIGNALED:
            case GL_CONDITION_SATISFIED:
                return;
            case GL_WAIT_FAILED:
                FAIL(glGetError());
            case GL_TIMEOUT_EXPIRED:
            default:
                FAIL(result);
            }
        }
    }
    SECTION("Multiple Fence") {
        auto fence1 = std::unique_ptr<std::remove_pointer_t<GLsync>, void (*)(GLsync)>{
            glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0), &glDeleteSync};
        auto fence2 = std::unique_ptr<std::remove_pointer_t<GLsync>, void (*)(GLsync)>{
            glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0), &glDeleteSync};
        // make some commands and flush
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();
        REQUIRE(glGetError() == GL_NO_ERROR);
        // both fence must be signaled
        GLenum result = GL_WAIT_FAILED;
        result = glClientWaitSync(fence1.get(), GL_SYNC_FLUSH_COMMANDS_BIT | 0, GL_TIMEOUT_IGNORED);
        REQUIRE(result == GL_ALREADY_SIGNALED);
        result = glClientWaitSync(fence2.get(), GL_SYNC_FLUSH_COMMANDS_BIT | 0, GL_TIMEOUT_IGNORED);
        REQUIRE(result == GL_ALREADY_SIGNALED);
    }
}

/// @see https://support.microsoft.com/en-us/help/124103/how-to-obtain-a-console-window-handle-hwnd
HWND get_hwnd_for_console() noexcept {
    constexpr auto max_length = 800;
    wchar_t title[max_length]{};
    GetConsoleTitleW(title, max_length);
    return FindWindowW(NULL, title);
}

TEST_CASE("console window handle", "[windows][!mayfail]") {
    if (get_hwnd_for_console() == NULL)
        FAIL("Faild to find HWND from console name");
}

TEST_CASE("EGLContext helper with Console", "[egl][!mayfail]") {
    HWND handle = get_hwnd_for_console();
    if (handle == NULL)
        FAIL("Faild to find HWND from console name");

    egl_context_t context{eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT};
    REQUIRE(context.is_valid());
    // we can't draw on console window.
    // for some reason ANGLE returns EGL_SUCCESS for this case ...
    REQUIRE(context.resume(handle) == EGL_BAD_NATIVE_WINDOW);
    REQUIRE(context.suspend() == EGL_SUCCESS);
}

// @todo Change to WinRT (or WinUI?)
struct win32_window_helper_t final {
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
    explicit win32_window_helper_t(HINSTANCE happ = GetModuleHandleW(NULL), LPCWSTR class_name = L"----")
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
    ~win32_window_helper_t() {
        DestroyWindow(this->window);
        UnregisterClassW(this->winclass.lpszClassName, nullptr);
    }

    bool consume_message(MSG& msg) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return msg.message != WM_QUIT;
    }

    // https://github.com/svenpilz/egl_offscreen_opengl/blob/master/egl_opengl_test.cpp
    uintptr_t run(win32_window_helper_t& app, EGLDisplay display, EGLSurface surface) noexcept(false) {
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
    win32_window_helper_t helper{};
    REQUIRE(helper.window);

    egl_context_t context{eglGetDisplay(EGL_DEFAULT_DISPLAY), EGL_NO_CONTEXT};
    REQUIRE(context.is_valid());
    REQUIRE(context.resume(helper.window) == EGL_SUCCESS);

    SECTION("fullspeed swap") {
        for (auto i = 0u; i < 256; ++i) {
            MSG msg{};
            if (helper.consume_message(msg) == false) // WM_QUIT
                break;
            SleepEx(5, TRUE);
            glClearColor(0, static_cast<float>(i) / 255, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            if (auto ec = glGetError())
                FAIL(ec);
            if (auto ec = context.swap(); ec != EGL_SUCCESS)
                FAIL(ec);
        }
    }
}

TEST_CASE("GL_PIXEL_UNPACK_BUFFER 1", "[egl][windows]") {
    win32_window_helper_t helper{};
    REQUIRE(helper.window);

    egl_context_t context{eglGetDisplay(EGL_DEFAULT_DISPLAY), eglGetCurrentContext()};
    REQUIRE(context.is_valid());
    REQUIRE(context.resume(helper.window) == EGL_SUCCESS);

    const auto length = static_cast<GLuint>(context.surface_width * context.surface_height * 4);
    REQUIRE(length);
    pbo_writer_t writer{length};
    REQUIRE(writer.is_valid() == GL_NO_ERROR);
}

auto start_opengl_test() -> gsl::final_action<void (*)()>;
auto create_opengl_window(gsl::czstring<> window_name, GLint width, GLint height) noexcept
    -> std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>;

struct glfw_test_case {
  protected:
    gsl::final_action<void (*)()> on_cleanup = start_opengl_test();
    std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> window{nullptr, nullptr};

  public:
    static void on_error(int code, const char* description) {
        spdlog::error("code {} message {}", code, description);
    }
    static void on_close(GLFWwindow* window) {
        spdlog::trace("closing GLFW window {}", static_cast<void*>(window));
    }

    glfw_test_case() {
        glfwSetErrorCallback(on_error);
        window = create_opengl_window("GLFW3", 800, 800);
        if (window == nullptr) {
            const char* message = nullptr;
            glfwGetError(&message);
            FAIL(message);
        }
        glfwSetWindowCloseCallback(window.get(), on_close);
    }
};

TEST_CASE_METHOD(glfw_test_case, "GLFW + OpenGL ES", "[opengl][glfw]") {
    glfwMakeContextCurrent(window.get());
    REQUIRE(eglGetDisplay(EGL_DEFAULT_DISPLAY) == eglGetCurrentDisplay());

    SECTION("poll event / swap buffer") {
        auto repeat = 120;
        while (glfwWindowShouldClose(window.get()) == false && repeat--) {
            glfwPollEvents();
            glClearColor(0, 0, static_cast<float>(repeat) / 255, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            // ...
            glfwSwapBuffers(window.get());
        }
    }
}

TEST_CASE_METHOD(glfw_test_case, "EGLContext helper with GLFW", "[egl][glfw]") {
    glfwMakeContextCurrent(window.get());
    REQUIRE(glfwGetWin32Window(window.get()));

    GIVEN("separate EGLContext") {
        egl_context_t context{glfwGetEGLDisplay(), glfwGetEGLContext(window.get())};
        REQUIRE(context.is_valid());

        WHEN("create EGLSurface and MakeCurrent")
        THEN("EGL_BAD_ALLOC") {
            // window is already created for the context
            REQUIRE(context.resume(glfwGetWin32Window(window.get())) == EGL_BAD_ALLOC);
        }
    }
    GIVEN("shared EGLContext") {
        EGLContext shared_context = glfwGetEGLContext(window.get());
        REQUIRE(shared_context == eglGetCurrentContext());
        egl_context_t context{glfwGetEGLDisplay(), shared_context};
        REQUIRE(context.is_valid());

        WHEN("create EGLSurface and MakeCurrent")
        THEN("EGL_BAD_ALLOC") {
            // window is already created for the context
            REQUIRE(context.resume(glfwGetWin32Window(window.get())) == EGL_BAD_ALLOC);
        }
    }
}

/// @see https://docs.gl/
TEST_CASE_METHOD(glfw_test_case, "GLFW info", "[glfw]") {
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

// see https://www.roxlu.com/2014/048/fast-pixel-transfers-with-pixel-buffer-objects
TEST_CASE_METHOD(glfw_test_case, "GL_PIXEL_PACK_BUFFER 1", "[opengl][glfw]") {
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

/// @see http://docs.gl/es3/glReadPixels
TEST_CASE_METHOD(glfw_test_case, "GL_PIXEL_PACK_BUFFER 2", "[opengl][glfw]") {
    glfwMakeContextCurrent(window.get());
    GLint frame[4]{};
    glGetIntegerv(GL_VIEWPORT, frame);
    REQUIRE(glGetError() == GL_NO_ERROR);
    REQUIRE(frame[2] * frame[3] > 0);

    // The other acceptable pair can be discovered by querying GL_IMPLEMENTATION_COLOR_READ_FORMAT and GL_IMPLEMENTATION_COLOR_READ_TYPE.
    pbo_reader_t reader{static_cast<GLuint>(frame[2] * frame[3] * 4)};
    if (auto ec = reader.is_valid())
        FAIL(ec);
    glReadBuffer(GL_BACK);
    REQUIRE(glGetError() == GL_NO_ERROR);

    GLint read_format{};
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &read_format);
    REQUIRE(glGetError() == GL_NO_ERROR);
    CAPTURE(read_format, GL_RGBA);

    GLint read_type{};
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &read_type);
    REQUIRE(glGetError() == GL_NO_ERROR);
    CAPTURE(read_type, GL_UNSIGNED_BYTE);

    reader_callback_t is_untouched = [](void*, const void* mapping, size_t) {
        REQUIRE(*reinterpret_cast<const uint32_t*>(mapping) == 0x00'00'00'00); // ABGR in 32 bpp
    };
    reader_callback_t is_blue = [](void*, const void* mapping, size_t) {
        REQUIRE(*reinterpret_cast<const uint32_t*>(mapping) == 0xFF'FF'00'00);
    };
    reader_callback_t is_blue_green = [](void*, const void* mapping, size_t) {
        REQUIRE(*reinterpret_cast<const uint32_t*>(mapping) == 0xFF'FF'FF'00);
    };

    GLuint fbo = 0; // use current window
    uint16_t count = 10;
    while (--count) {
        auto const front = static_cast<uint16_t>((count + 1) % 2);
        auto const back = static_cast<uint16_t>(count % 2);
        // perform rendering...
        // 1st --> blue_green
        // 2nd --> blue
        // 3rd --> blue_green
        // ...
        glClearColor(0, static_cast<float>(back), 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // pack to the back buffer(GL_BACK)
        if (auto ec = reader.pack(back, fbo, frame))
            FAIL(ec);
        // map the front buffer and read
        if (count == 9) {
            // if there was no read, the pixel buffer object is clean and must hold zero (0,0,0,0)
            // this is the first read, so it will be untouched
            if (auto ec = reader.map_and_invoke(front, is_untouched, nullptr))
                FAIL(ec);
        } else {
            // if back == 0, (2nd, 4th... read) the clear color of previous frame is (0,1,1,1). this is blue_green.
            // if back == 1, (3rd, 5rh... read) the color is (0,0,1,1), which is blue.
            if (auto ec = reader.map_and_invoke(front, back == 0 ? is_blue_green : is_blue, nullptr))
                FAIL(ec);
        }
        glfwSwapBuffers(window.get());
    }
}

auto start_opengl_test() -> gsl::final_action<void (*)()> {
    REQUIRE(glfwInit());
    return gsl::finally(&glfwTerminate);
}

/**
 * @see https://www.glfw.org/docs/latest/window_guide.html#GLFW_CONTEXT_CREATION_API_hint
 */
auto create_opengl_window(gsl::czstring<> window_name, GLint width, GLint height) noexcept
    -> std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> {
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
    return std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>{glfwCreateWindow(width, height, window_name, NULL, NULL),
                                                              &glfwDestroyWindow};
}
