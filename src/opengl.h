#pragma once
#include <gsl/gsl>
#include <string_view>
#include <system_error>

#if defined(__APPLE__)
#define __gl_h_ // suppress <OpenGL/gl.h>
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/OpenGL.h> // _OPENGL_H
#include <OpenGL/gl3.h>    // __gl3_h_
#include <OpenGL/gl3ext.h> // __gl3ext_h_

#else
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2ext_angle.h>
#endif

/**
 * @brief Generate a context which doesn't require surface for rendering
 */
auto make_offscreen_context(uint32_t& ec, std::string& message) noexcept
    -> std::shared_ptr<void>;

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
                    GLuint shader,
                    GLenum status_name = GL_COMPILE_STATUS) noexcept;
    static bool                            //
    get_program_info(std::string& message, //
                     GLuint program,
                     GLenum status_name = GL_LINK_STATUS) noexcept;
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
