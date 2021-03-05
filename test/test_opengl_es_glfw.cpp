#include <catch2/catch.hpp>
#include <gsl/gsl>
#include <spdlog/spdlog.h>

#include <opengl_1.h>
#define GLFW_INCLUDE_ES3
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

using namespace std;

auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger>;
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

    REQUIRE(eglGetCurrentContext() != EGL_NO_CONTEXT);
    REQUIRE(glGetString(GL_SHADING_LANGUAGE_VERSION));
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

// see https://www.roxlu.com/2014/048/fast-pixel-transfers-with-pixel-buffer-objects
TEST_CASE("OpenGL PixelBuffer", "[opengl][glfw]") {
    auto stream = get_current_stream();
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
