#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>

#include <graphics.h>

#if defined(_WIN32)
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
// clang-format on
#endif

#define EGL_PROC(fp, name) auto fp = reinterpret_cast<decltype(&name)>(eglGetProcAddress(#name));
#undef EGL_PROC

class EGLConfigTestCase1 {
  protected:
    EGLDisplay display;
    EGLint major = 0, minor = 0;

  public:
    EGLConfigTestCase1() {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        REQUIRE(display != EGL_NO_DISPLAY);
        if (eglInitialize(display, &major, &minor) == false)
            REQUIRE(eglGetError() == EGL_SUCCESS);
        REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));
    }
    ~EGLConfigTestCase1() {
        REQUIRE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        REQUIRE(eglTerminate(display));
    }
};

TEST_CASE_METHOD(EGLConfigTestCase1, "ChooseConfig", "[egl]") {
    EGLint count = 0;
    if (eglChooseConfig(display, nullptr, nullptr, 0, &count) == false)
        REQUIRE(eglGetError() == EGL_SUCCESS);
    REQUIRE(count);
    auto configs = std::make_unique<EGLConfig[]>(count);
    REQUIRE(eglChooseConfig(display, nullptr, configs.get(), count, &count));
}
