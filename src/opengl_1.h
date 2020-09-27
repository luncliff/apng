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

/**
 * @brief EGL Context and other informations + RAII
 */
struct egl_helper_t {
    EGLNativeWindowType native_window{};
    EGLNativeDisplayType native_display = EGL_DEFAULT_DISPLAY;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLint major = 0, minor = 0;
    EGLint count = 0;
    EGLConfig configs[10]{};
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;

  public:
    explicit egl_helper_t(EGLNativeDisplayType native) noexcept(false);
    egl_helper_t(EGLNativeWindowType native, bool is_console) noexcept(false);
    ~egl_helper_t() noexcept;
};

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

using readback_callback_t = void (*)(void* user_data, const void* mapping, size_t length);

class opengl_readback_t final {
  private:
    GLuint pbos[2]{};
    const uint16_t capacity = 2;
    uint32_t length = 0; // length of the buffer modification
    GLintptr offset = 0;

  public:
    opengl_readback_t(GLint w, GLint h) noexcept(false);
    ~opengl_readback_t() noexcept(false);

    /// @brief fbo -> pbo[idx]
    GLenum pack(uint16_t idx, GLuint fbo, const GLint frame[4]) noexcept;
    GLenum map_and_invoke(uint16_t idx, readback_callback_t callback, void* user_data) noexcept;
};

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
