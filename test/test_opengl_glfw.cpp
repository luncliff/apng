#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>
#include <filesystem>

#include "programs.h"
#include "service.hpp"

#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

namespace fs = std::filesystem;

auto create_window(gsl::czstring<> window_name) noexcept
    -> std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>;
auto start_opengl_test() {
    REQUIRE(glfwInit());
    return gsl::finally(&glfwTerminate);
}

TEST_CASE("GLFW Window", "[opengl]") {
    auto on_return = start_opengl_test();
    auto window = create_window("window0");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());

    SECTION("GL_SHADING_LANGUAGE_VERSION") {
        auto txt = reinterpret_cast<const char*>(
            glGetString(GL_SHADING_LANGUAGE_VERSION));
        REQUIRE(txt);
    }
    SECTION("has context") {
        auto const context = glfwGetCurrentContext();
        REQUIRE(context);
    }
    SECTION("basic loop") {
        auto repeat = 60;
        while (glfwWindowShouldClose(window.get()) == false && repeat--) {
            glfwPollEvents();
            glClearColor(0, static_cast<float>(repeat) / 255, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            if (auto ec = glGetError())
                FAIL(ec);
            glfwSwapBuffers(window.get());
        }
    }
}

TEST_CASE("window - texture2d_renderer_t", "[opengl]") {
    auto on_return = start_opengl_test();
    auto window = create_window("window1");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());

    opengl_texture_t tex{};
    texture2d_renderer_t renderer{};

    GLint w = -1, h = -1;
    glfwGetFramebufferSize(window.get(), &w, &h);
    tex.update(static_cast<uint16_t>(w / 2), static_cast<uint16_t>(h / 2),
               nullptr);
    glViewport(0, 0, w, h);
    auto const context = glfwGetCurrentContext();
    auto repeat = 60;
    while (glfwWindowShouldClose(window.get()) == false && repeat--) {
        glfwPollEvents();
        glClearColor(0, static_cast<float>(repeat) / 255, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        if (auto ec = renderer.render(context, tex.name, tex.target))
            FAIL(ec);
        glfwSwapBuffers(window.get());
    }
}

uint32_t create(const fs::path& fpath, void* blob, size_t blob_size) {
    auto fp = create(fpath);
    if (fp == nullptr)
        return errno;
    const auto wsz = fwrite(blob, blob_size, 1, fp.get());
    if (wsz == 0)
        return errno;
    return 0;
};

TEST_CASE("offscreen - texture2d_renderer_t", "[opengl]") {
    std::string message{};
    uint32_t ec = 0;
    auto context = make_offscreen_context(ec, message);
    if (context == nullptr) {
        INFO(message);
        FAIL(ec);
    }

    GLint w = 300, h = 300;
    opengl_framebuffer_t fb{static_cast<uint16_t>(w), static_cast<uint16_t>(h)};
    REQUIRE(fb.bind() == GL_NO_ERROR);
    REQUIRE(glIsFramebuffer(fb.name));
    REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
            GL_FRAMEBUFFER_COMPLETE);

    texture2d_renderer_t renderer{};
    opengl_texture_t tex{};
    REQUIRE(tex.update(static_cast<uint16_t>(w), static_cast<uint16_t>(h),
                       nullptr) == GL_NO_ERROR);

    glViewport(0, 0, w, h);
    glClearColor(1, 1, 1, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    REQUIRE(renderer.render(nullptr, tex.name) == GL_NO_ERROR);

    /// @todo http://www.imagemagick.org/Usage/compare/#difference
    SECTION("glReadPixels") {
        auto buf = std::make_unique<uint32_t[]>(w * h);
        REQUIRE(fb.read_pixels(w, h, buf.get()) == GL_NO_ERROR);
        REQUIRE(create("offscreen_1.bin", buf.get(), w * h * 4) == 0);
    }
}

void configure_glfw() noexcept {
#if defined(GLFW_INCLUDE_ES2) || defined(GLFW_INCLUDE_ES3)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#if defined(GLFW_INCLUDE_ES2)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2); // ES 2.0
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#elif defined(GLFW_INCLUDE_ES3)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // ES 3.1
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#endif
#elif defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
#endif
}
/**
 * @see https://www.glfw.org/docs/latest/window_guide.html#GLFW_CONTEXT_CREATION_API_hint
 */
auto create_window(gsl::czstring<> window_name) noexcept
    -> std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> {
    configure_glfw();
    return std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>{
        glfwCreateWindow(160 * 5, 160 * 5, //
                         window_name,      //
                         NULL, NULL),
        &glfwDestroyWindow};
}
