#pragma once
#include <filesystem>
#include <gsl/gsl>
#include <string_view>
#include <system_error>
// #include <memory_resource>

#if __has_include(<angle_gl.h>)
#include <angle_gl.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GLES3/gl3.h>
#if __has_include(<GLES3/gl31.h>)
#include <GLES3/gl31.h>
#endif
#endif
#if __has_include(<EGL/egl.h>)
#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>

#endif // <EGL/egl.h>

namespace fs = std::filesystem;

auto open(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)>;
auto create(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)>;

/**
 * @brief `std::error_category` for `std::system_error` in this module
 * @see   `std::system_error`
 * @see   `std::error_category`
 */
std::error_category& get_opengl_category() noexcept;

/**
 * @brief OpenGL Vertex Array Object + RAII
 */
struct opengl_vao_t final {
    GLuint name{};

  public:
    opengl_vao_t() noexcept;
    ~opengl_vao_t() noexcept;
};

/**
 * @brief OpenGL Shader Program + RAII
 */
struct opengl_shader_program_t final {
    GLuint id{}, vs{}, fs{};

  public:
    opengl_shader_program_t(std::string_view vtxt, //
                            std::string_view ftxt) noexcept(false);
    ~opengl_shader_program_t() noexcept;

    operator bool() const noexcept;
    GLint uniform(gsl::czstring<> name) const noexcept;
    GLint attribute(gsl::czstring<> name) const noexcept;

  private:
    static GLuint create_compile_attach(GLuint program, GLenum shader_type, //
                                        std::string_view code) noexcept(false);
    static bool                           //
    get_shader_info(std::string& message, //
                    GLuint shader, GLenum status_name = GL_COMPILE_STATUS) noexcept;
    static bool                            //
    get_program_info(std::string& message, //
                     GLuint program, GLenum status_name = GL_LINK_STATUS) noexcept;
};

/**
 * @brief OpenGL Texture + RAII
 */
struct opengl_texture_t final {
    GLuint name{};
    GLenum target{};

  public:
    opengl_texture_t() noexcept(false);
    opengl_texture_t(GLuint name, GLenum target) noexcept(false);
    ~opengl_texture_t() noexcept(false);

    operator bool() const noexcept;
    GLenum update(uint16_t width, uint16_t height, const void* ptr) noexcept;
};

/**
 * @brief OpenGL FrameBuffer + RAII
 * @code
 * glBindFramebuffer(GL_FRAMEBUFFER, fb.name);
 * @endcode
 */
struct opengl_framebuffer_t final {
    GLuint name{};
    GLuint buffers[2]{}; // color, depth
  public:
    opengl_framebuffer_t(uint16_t width, uint16_t height) noexcept(false);
    ~opengl_framebuffer_t() noexcept;

    GLenum bind(void* context = nullptr) noexcept;
    GLenum read_pixels(uint16_t width, uint16_t height, void* pixels) noexcept;
};
