#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>

#include <gsl/gsl>
#include <type_traits>

#if defined(__APPLE__)
#define GLFW_INCLUDE_ES2
#else
#define GLFW_INCLUDE_ES3
#endif
#define GLFW_INCLUDE_GLEXT
#include <GLFW/glfw3.h>

#include "opengl_es.h"
#include "programs.h"

using namespace std;

auto create_test_window(const char* window_name) -> shared_ptr<GLFWwindow>;

class gfx_program_t {
  public:
    virtual ~gfx_program_t() noexcept = default;

  public:
    virtual void setup(EGLContext context) noexcept(false) = 0;
    virtual void teardown(EGLContext context) noexcept(false) = 0;

    virtual void update(EGLContext context, //
                        uint16_t width, uint16_t height) noexcept(false) = 0;
    virtual GLenum draw(EGLContext context) = 0;
};

auto create_tex2d_program() -> unique_ptr<gfx_program_t>;

TEST_CASE("GL Framework + OpenGL", "[opengl]") {
    auto window = create_test_window("OpenGL ES");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());
    const auto context = eglGetCurrentContext();
    REQUIRE(context != EGL_NO_CONTEXT);

    SECTION("swap buffer / poll event") {
        auto repeat = 10;
        while (glfwWindowShouldClose(window.get()) == false && repeat--) {
            glClear(GL_COLOR_BUFFER_BIT);
            // ...
            glfwSwapBuffers(window.get());
            glfwPollEvents();
        }
    }
}

void run_program(GLFWwindow* window, EGLContext context, //
                 gfx_program_t& gfx, uint16_t repeat = 60) noexcept(false) {
    gfx.setup(context);
    auto on_return = gsl::finally([&gfx, context]() {
        gfx.teardown(context); // cleanup
    });
    while (glfwWindowShouldClose(window) == false && repeat--) {
        // update
        GLint w = -1, h = -1;
        glfwGetFramebufferSize(window, &w, &h);
        gfx.update(context, (uint16_t)w, (uint16_t)h);
        // clear / draw call
        const auto color = static_cast<float>(repeat) / 255;
        glClearColor(color, color, color, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (auto ec = gfx.draw(context))
            FAIL(get_opengl_category().message(ec));
        // present
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

TEST_CASE("tex2d_renderer_t", "[opengl]") {
    auto window = create_test_window("OpenGL ES");
    if (window == nullptr) {
        const char* message = nullptr;
        glfwGetError(&message);
        FAIL(message);
    }
    glfwMakeContextCurrent(window.get());
    const auto context = eglGetCurrentContext();
    REQUIRE(context != EGL_NO_CONTEXT);
    unique_ptr<gfx_program_t> program = nullptr;

    SECTION("GL_TEXTURE_2D") {
        class impl_t final : public gfx_program_t {
            unique_ptr<texture2d_renderer_t> renderer{};
            unique_ptr<opengl_texture_t> tex{};

          public:
            ~impl_t() noexcept = default;

          public:
            void setup(EGLContext) noexcept(false) override {
                renderer = make_unique<texture2d_renderer_t>();
            }
            void teardown(EGLContext) noexcept(false) override {
                auto _renderer = move(renderer);
                auto _tex = move(tex);
            }
            void update(EGLContext, //
                        uint16_t w, uint16_t h) noexcept(false) override {
                glViewport(0, 0, w, h);
                if (tex == nullptr)
                    tex = make_unique<opengl_texture_t>();
                if (int ec = tex->update(w, h, nullptr))
                    throw system_error{ec, get_opengl_category(),
                                       "glTexImage2D"};
            }

            GLenum draw(EGLContext context) override {
                GLint viewport[4]{};
                glGetIntegerv(GL_VIEWPORT, viewport);
                if (auto ec = glGetError())
                    return ec;
                return renderer->render(context, tex->name, tex->target);
            }
        };
        program = make_unique<impl_t>();
    }
    REQUIRE(program != nullptr);
    return run_program(window.get(), context, *program);
}

void configure_glfw() noexcept;
auto create_test_window(const char* window_name) -> shared_ptr<GLFWwindow> {
    if (glfwInit() == GLFW_FALSE)
        return nullptr;
    configure_glfw();
    return shared_ptr<GLFWwindow>{
        glfwCreateWindow(480, 480, window_name, NULL, NULL),
        [](GLFWwindow* window) {
            glfwDestroyWindow(window);
            glfwTerminate();
        }};
}
