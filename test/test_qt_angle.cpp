#if defined(_WIN32)
// #define CATCH_CONFIG_WINDOWS_CRTDBG
#endif
#define CATCH_CONFIG_BENCHMARK
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <catch2/catch_reporter_sonarqube.hpp>

#include <gsl/gsl>
#if defined(_WIN32)
#include <winrt/base.h>

#include <d3d11_2.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#include <DirectXTK/WICTextureLoader.h> // #include <d3d11_1.h>
#endif

#if __has_include(<QtOpenGL/qgl.h>) // from Qt5::OpenGL
// #define GL_GLEXT_PROTOTYPES
#include <QtOpenGL/qglfunctions.h>
// #define EGL_EGLEXT_PROTOTYPES
#include <QtANGLE/EGL/egl.h>
#include <QtANGLE/EGL/eglext.h>
#include <QtANGLE/EGL/eglext_angle.h>

#elif __has_include(<angle_gl.h>) // from google/ANGLE
#define GL_GLEXT_PROTOTYPES
#include <angle_gl.h>
#include <angle_windowsstore.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>

#endif

class TestCase1 {
  protected:
    winrt::com_ptr<ID3D11Device> device{};
    winrt::com_ptr<ID3D11DeviceContext> device_context{};
};

TEST_CASE_METHOD(TestCase1, "usage 1", "[windows]") {
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_1;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, nullptr, 0, //
                                   D3D11_SDK_VERSION, device.put(), &level, device_context.put());
    if FAILED (hr)
        FAIL(hr);
    REQUIRE(device);
    REQUIRE(device->GetFeatureLevel() == level);
}

#define EGL_PROC(name) reinterpret_cast<decltype(&name)>(eglGetProcAddress(#name));

class TestCase2 {
  protected:
    EGLDisplay display;

  public:
    TestCase2() {
        auto fGetDisplay = EGL_PROC(eglGetDisplay);
        REQUIRE(fGetDisplay);
        display = fGetDisplay(EGL_DEFAULT_DISPLAY);
        REQUIRE(display != EGL_NO_DISPLAY);
    }
    ~TestCase2() {
        REQUIRE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        REQUIRE(eglTerminate(display));
    }
};

TEST_CASE_METHOD(TestCase2, "usage 2", "[egl]") {
    auto feglGetError = EGL_PROC(eglGetError);
    auto feglInitialize = EGL_PROC(eglInitialize);
    auto feglBindAPI = EGL_PROC(eglBindAPI);

    EGLint major = 0, minor = 0;
    if (feglInitialize(display, &major, &minor) == false)
        REQUIRE(feglGetError() == EGL_SUCCESS);
    REQUIRE(feglBindAPI(EGL_OPENGL_ES_API));
    REQUIRE(major >= 1);
    REQUIRE(minor >= 4);

    SECTION("eglChooseConfig") {
        auto feglChooseConfig = EGL_PROC(eglChooseConfig);

        EGLint count = 0;
        EGLConfig configs[10]{};
        if (feglChooseConfig(display, nullptr, configs, 10, &count) == false)
            REQUIRE(feglGetError() == EGL_SUCCESS);
        CAPTURE(count);
    }
}
