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

#define LOCAL_EGL_PROC(fn, name) auto fn = reinterpret_cast<decltype(&name)>(eglGetProcAddress(#name));

/**
 * @brief EGLConfigTestCase for QtANGLE
 * 
 * @see https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglChooseConfig.xhtml
 * @see https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglGetConfigs.xhtml
 * @see https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglGetConfigAttrib.xhtml
 * @todo https://github.com/android/ndk-samples/blob/master/native-activity/app/src/main/cpp/main.cpp
 */
class EGLConfigTestCase2 {
  protected:
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;

  public:
    EGLConfigTestCase2() {
        REQUIRE(display != EGL_NO_DISPLAY);
        if (eglInitialize(display, &major, &minor) == false)
            REQUIRE(eglGetError() == EGL_SUCCESS);
        REQUIRE(eglBindAPI(EGL_OPENGL_ES_API));
    }
    ~EGLConfigTestCase2() {
        REQUIRE(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
        if (display != eglGetDisplay(EGL_DEFAULT_DISPLAY))
            REQUIRE(eglTerminate(display));
    }
};

/// @see https://github.com/google/angle/blob/master/src/libANGLE/Config.cpp
void print1(EGLDisplay display, EGLConfig config) {
    EGLint value;
    REQUIRE(eglGetConfigAttrib(display, config, EGL_CONFIG_ID, &value));
    spdlog::info("  - EGL_CONFIG_ID: {}", value);
#define PRINT_IF_SUPPORTED(attribute)                                                                                  \
    if (eglGetConfigAttrib(display, config, attribute, &value) == EGL_FALSE)                                           \
        spdlog::debug("    {} --> {}", #attribute, eglGetError()); /* required for both debug and state clear */       \
    else {                                                                                                             \
        spdlog::info("    {}: {}", #attribute, value);                                                                 \
        value = 0;                                                                                                     \
    }
    //PRINT_IF_SUPPORTED(EGL_CONFIG_CAVEAT)
    //PRINT_IF_SUPPORTED(EGL_NATIVE_VISUAL_ID)
    //PRINT_IF_SUPPORTED(EGL_NATIVE_VISUAL_TYPE)
    PRINT_IF_SUPPORTED(EGL_SURFACE_TYPE)
    //PRINT_IF_SUPPORTED(EGL_TRANSPARENT_TYPE)
    PRINT_IF_SUPPORTED(EGL_SAMPLES)
    PRINT_IF_SUPPORTED(EGL_MAX_PBUFFER_WIDTH)
    PRINT_IF_SUPPORTED(EGL_MAX_PBUFFER_HEIGHT)
    //PRINT_IF_SUPPORTED(EGL_MAX_PBUFFER_PIXELS)
    PRINT_IF_SUPPORTED(EGL_BLUE_SIZE)
    PRINT_IF_SUPPORTED(EGL_GREEN_SIZE)
    PRINT_IF_SUPPORTED(EGL_RED_SIZE)
    PRINT_IF_SUPPORTED(EGL_ALPHA_SIZE)
    PRINT_IF_SUPPORTED(EGL_DEPTH_SIZE)
    PRINT_IF_SUPPORTED(EGL_STENCIL_SIZE)
#undef PRINT_IF_SUPPORTED
}

TEST_CASE_METHOD(EGLConfigTestCase2, "ChooseConfig", "[egl]") {
    EGLint count = 0;
    if (eglChooseConfig(display, nullptr, nullptr, 0, &count) == false)
        REQUIRE(eglGetError() == EGL_SUCCESS);
    REQUIRE(count);
    auto configs = std::make_unique<EGLConfig[]>(count);
    REQUIRE(eglChooseConfig(display, nullptr, configs.get(), count, &count));

    spdlog::info("EGLConfig:");
    for (auto i = 0; i < count; ++i) {
        print1(display, configs[i]);
    }
}
