#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>
#include <gsl/gsl>

#include <iostream>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GLFW_INCLUDE_ES3
//#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#include "gles_with_angle.h"

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
            std::cout << extname << std::endl;
            offset = ++i;
        }
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
        egl.mContext = eglCreateContext(egl.mDisplay, 
            egl.mConfig, EGL_NO_CONTEXT, NULL);
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

// // https://github.com/svenpilz/egl_offscreen_opengl/blob/master/egl_opengl_test.cpp
// uint32_t run(app_context_t& app, //
//              egl_bundle_t& egl, uint32_t repeat = INFINITE) noexcept(false) {
//     MSG msg{};
//     while ((app.consume_message(msg) == false) && repeat--) {
//         // update
//         SleepEx(10, TRUE);
//         // clear/render
//         glClearColor(37.0f / 255, 27.0f / 255, 82.0f / 255, 1);
//         glClear(GL_COLOR_BUFFER_BIT);
//         // ...
//         if (eglSwapBuffers(egl.mDisplay, egl.mSurface) == false)
//             throw std::runtime_error{"eglSwapBuffers(mDisplay, mSurface"};
//     }
//     return msg.wParam;
// }
