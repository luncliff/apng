#pragma once
#include <gsl/gsl>
#include <string_view>
#include <system_error>

// OpenGL ES / ANGLE
#include <GLES3/gl3.h>
//#include <GLES3/gl31.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#if __has_include(<EGL/eglext_angle.h>)
#include <EGL/eglext_angle.h>
#include <EGL/eglplatform.h>
#endif

/**
 * @brief EGL Context and other informations + RAII
 */
struct egl_bundle_t {
    EGLNativeWindowType native_window{};
    EGLNativeDisplayType native_display = EGL_DEFAULT_DISPLAY;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = 0;
    EGLint major, minor;

  public:
    explicit egl_bundle_t(EGLNativeDisplayType native //
                          = EGL_DEFAULT_DISPLAY) noexcept(false);
    egl_bundle_t(EGLNativeWindowType native, //
                 bool is_console) noexcept(false);
    ~egl_bundle_t() noexcept;
};

std::error_category& get_gles_category() noexcept;

/**
 * @brief OpenGL Vertex Array Object + RAII
 */
struct vao_t {
    GLuint name;

  public:
    vao_t() noexcept;
    ~vao_t() noexcept;
};

/**
 * @brief OpenGL Shader Program + RAII
 */
struct program_t {
    GLuint id, vs, fs;

  public:
    program_t(std::string_view vtxt, //
              std::string_view ftxt) noexcept(false);
    ~program_t() noexcept;

    operator bool() const noexcept;
    GLint uniform(gsl::czstring<> name) const noexcept;
    GLint attribute(gsl::czstring<> name) const noexcept;
};

/**
 * @brief OpenGL Texture + RAII
 */
struct texture_t final {
    GLuint name;
    GLenum target;

  public:
    texture_t(GLuint name, GLenum target) noexcept(false);
    // GL_TEXTURE_2D
    texture_t(uint16_t width, uint16_t height, uint32_t* ptr) noexcept(false);
    ~texture_t() noexcept(false);

  public:
    GLenum update(uint16_t width, uint16_t height, uint32_t* ptr) noexcept;
};

/**
 * @brief OpenGL FrameBuffer + RAII
 * @code
 * glBindFramebuffer(GL_FRAMEBUFFER, fb.name);
 * @endcode
 */
struct framebuffer_t {
    GLuint name;
    GLuint buffers[2]{}; // color, depth
  public:
    framebuffer_t(uint16_t width, uint16_t height) noexcept(false);
    ~framebuffer_t() noexcept;

    GLenum bind() noexcept;
};

struct tex2d_renderer_t {
    vao_t vao;
    program_t program;
    GLuint vbo, ebo;

  public:
    tex2d_renderer_t() noexcept(false);
    ~tex2d_renderer_t() noexcept(false);

    GLenum render(EGLContext context, GLuint texture, GLenum target) noexcept;
    GLenum unbind(EGLContext context) noexcept;
    GLenum bind(EGLContext context) noexcept;
};
