#include <catch2/catch.hpp>

#include <gsl/gsl>
#include <type_traits>

#include <opengl_1.h>
#define GLFW_INCLUDE_ES3
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

using namespace std;

auto start_opengl_test() -> gsl::final_action<void (*)()>;
auto create_opengl_window(gsl::czstring<> window_name) noexcept -> std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>;

TEST_CASE("GLFW + OpenGL ES", "[opengl][glfw]") {
    auto on_return = start_opengl_test();
    auto window = create_opengl_window("OpenGL ES");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());

    SECTION("eglGetCurrentContext") {
        REQUIRE(eglGetCurrentContext() != EGL_NO_CONTEXT);
        REQUIRE(glGetString(GL_SHADING_LANGUAGE_VERSION));
    }
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

TEST_CASE("OpenGL ES resources", "[opengl][glfw]") {
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

    SECTION("Pixel Buffer Objects") {
        GLuint pbos[2]{}; // 0 - GL_STREAM_READ, 1 - GL_STREAM_DRAW
        glGenBuffers(2, pbos);
        REQUIRE(glGetError() == GL_NO_ERROR);
        auto on_return = gsl::finally([pbos]() {
            glDeleteBuffers(2, pbos);
            REQUIRE(glGetError() == GL_NO_ERROR);
        });
        // 0 is for read (download)
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[0]);
        glBufferData(GL_PIXEL_PACK_BUFFER, w * h * 4, nullptr, GL_STREAM_READ);
        REQUIRE(glGetError() == GL_NO_ERROR);
        const void* mapping0 = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, w * h * 4, //
                                                GL_MAP_READ_BIT);
        REQUIRE(glGetError() == GL_NO_ERROR);
        REQUIRE(mapping0);
        REQUIRE(glUnmapBuffer(GL_PIXEL_PACK_BUFFER));

        // 1 is for write (upload)
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[1]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, w * h * 4, nullptr, GL_STREAM_DRAW);
        REQUIRE(glGetError() == GL_NO_ERROR);
        const void* mapping1 = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, w * h * 4, //
                                                GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
        REQUIRE(glGetError() == GL_NO_ERROR);
        REQUIRE(mapping1);
        REQUIRE(glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER));
    }
}

auto start_opengl_test() -> gsl::final_action<void (*)()> {
    REQUIRE(glfwInit());
    return gsl::finally(&glfwTerminate);
}

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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // ES 3.1
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#endif
#endif // defined(GLFW_INCLUDE_ES2) || defined(GLFW_INCLUDE_ES3)
#endif // __APPLE__ || _WIN32
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    const auto axis = 160 * 5;
    return std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>{glfwCreateWindow(axis, axis, window_name, NULL, NULL),
                                                              &glfwDestroyWindow};
}
