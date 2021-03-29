/**
 * @author Park DongHa (luncliff@gmail.com)
 */
#pragma once
#include <gsl/gsl>
#include <winrt/Windows.Foundation.h>

#include <filesystem>
#include <memory_resource>
#include <string_view>
#include <system_error>
// clang-format off
#if __has_include(<vulkan/vulkan.h>)
#  include <vulkan/vulkan.h>
#endif

#if __has_include(<QtOpenGL/qgl.h>) // from Qt5::OpenGL
#  include <QtOpenGL/qgl.h>
#  include <QtOpenGL/qglfunctions.h>
#  include <QtANGLE/GLES3/gl3.h>
#  include <QtANGLE/EGL/egl.h>
#  include <QtANGLE/EGL/eglext.h>
//#  include <QtANGLE/EGL/eglext_angle.h>
#elif __has_include(<angle_gl.h>) // from google/ANGLE
#  include <angle_gl.h>
#  if __has_include(<angle_windowsstore.h>)
#    include <angle_windowsstore.h>
#  endif
#  include <EGL/egl.h>
#  include <EGL/eglext.h>
#else
#  include <GLES3/gl3.h>
#  if __has_include(<GLES3/gl31.h>)
#    include <GLES3/gl31.h>
#  endif
#  include <EGL/egl.h>
#  include <EGL/eglext.h>
#endif
// clang-format on

/**
 * @brief `std::error_category` for `std::system_error` in this module
 * @see   `std::system_error`
 * @see   `std::error_category`
 */
std::error_category& get_opengl_category() noexcept;

class egl_context_t final {
  private:
    gsl::owner<EGLDisplay> display = EGL_NO_DISPLAY; // this will be EGL_NO_DISPLAY when `terminate`d
    uint16_t major = 0, minor = 0;
    gsl::owner<EGLContext> context = EGL_NO_CONTEXT;
    EGLConfig configs[3]{};
    gsl::owner<EGLSurface> surface = EGL_NO_SURFACE; // EGLSurface for Draw/Read
    int32_t surface_width = 0;
    int32_t surface_height = 0;

  public:
    /**
     * @brief Acquire EGLDisplay and create an EGLContext for OpenGL ES 3.0+
     * 
     * @see eglInitialize https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglInitialize.xhtml
     * @see eglChooseConfig
     * @see eglCreateContext
     */
    egl_context_t(EGLDisplay display, EGLContext share_context) noexcept;
    /**
     * @see terminate
     */
    ~egl_context_t() noexcept;
    egl_context_t(egl_context_t const&) = delete;
    egl_context_t& operator=(egl_context_t const&) = delete;
    egl_context_t(egl_context_t&&) = delete;
    egl_context_t& operator=(egl_context_t&&) = delete;

    /**
     * @brief EGLContext == NULL?
     * @details It is recommended to invoke this function to check whether the construction was successful.
     *          Notice that the constructor is `noexcept`.
     */
    bool is_valid() const noexcept;

    /**
     * @brief Destroy all EGL bindings and resources
     * @note This functions in invoked in the destructor
     * @post is_valid() == false
     * 
     * @see eglMakeCurrent
     * @see eglDestroyContext
     * @see eglDestroySurface
     * @see eglTerminate(unused) https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglTerminate.xhtml
     */
    void terminate() noexcept;

    /**
     * @brief   Create EGLSurface with given EGLNativeWindowType and bind with EGLContext
     * @return  EGLint  Redirected from `eglGetError`. 
     *                  `EGL_NOT_INITIALIZED` if `terminate` is invoked.
     * @see eglCreateWindowSurface
     * @see eglQuerySurface
     * @see eglMakeCurrent
     */
    EGLint resume(gsl::not_null<EGLNativeWindowType> window) noexcept;

    /**
     * @brief   Unbind EGLSurface and EGLContext.
     * 
     * @return EGLint   Redirected from `eglGetError`. 
     *                  `EGL_NOT_INITIALIZED` if `terminate` is invoked.
     * @see eglMakeCurrent
     * @see eglDestroySurface
     */
    EGLint suspend() noexcept;

    /**
     * @brief   Try to swap front/back buffer. 
     * @note    This function invokes `terminate` if EGL_BAD_CONTEXT/EGL_CONTEXT_LOST.
     * @return  EGLint  Redirected from `eglGetError`. 
     * @see eglSwapBuffers
     * @see terminate
     */
    EGLint swap() noexcept;

  private:
    static EGLint get_configs(EGLDisplay display, EGLConfig* configs, EGLint& count) noexcept;
};

/// @see memcpy
using reader_callback_t = void (*)(void* user_data, const void* mapping, size_t length);

/**
 * @see http://docs.gl/es3/glReadPixels 
 * 
 * @todo Test rendering surfaces with normalized fixed point - GL_RGBA/GL_UNSIGNED_BYTE
 * @todo Test rendering surfaces with signed integer - GL_RGBA_INTEGER/GL_INT
 * @todo Test rendering surfaces with for unsigned integer - GL_RGBA_INTEGER/GL_UNSIGNED_INT
 *
 * @see GL_EXT_map_buffer_range https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_map_buffer_range.txt
 */
class framebuffer_reader_t final {
    static constexpr uint16_t capacity = 2;

  private:
    GLuint pbos[capacity];
    uint32_t length; // byte length of the buffer modification
    GLintptr offset;
    GLenum ec = GL_NO_ERROR;

  public:
    explicit framebuffer_reader_t(GLuint length) noexcept;
    ~framebuffer_reader_t() noexcept;
    framebuffer_reader_t(framebuffer_reader_t const&) = delete;
    framebuffer_reader_t& operator=(framebuffer_reader_t const&) = delete;
    framebuffer_reader_t(framebuffer_reader_t&&) = delete;
    framebuffer_reader_t& operator=(framebuffer_reader_t&&) = delete;

    /**
     * @brief check whether the construction was successful
     * @return GLenum   cached `ec` from the constructor
     */
    GLenum is_valid() const noexcept;

    /**
     * @brief fbo -> pbo[idx]
     * 
     * @param idx   index of the pixel buffer object to receive pixels
     * @param fbo   target framebuffer object to run `glReadPixels`
     * @param frame area for `glReadPixels`
     * @return GLenum   GL_INVALID_VALUE if `idx` is wrong.
     *                  GL_OUT_OF_MEMORY if `frame` is larger than `length`.
     *                  Or, redirected from `glGetError` for the other cases.
     * @see glReadPixels  http://docs.gl/es3/glReadPixels
     */
    GLenum pack(uint16_t idx, GLuint fbo, const GLint frame[4], //
                GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE) noexcept;

    /**
     * @brief create a mapping for pbo[idx] and invoke the `callback`
     * @note  the mapping will be destroyed when the function returns

     * @param idx   index of the pixel buffer object to create temporary mapping
     * @return GLenum   GL_INVALID_VALUE if `idx` is wrong.
     *                  Or, redirected from `glGetError`
     * @see glBindBuffer
     * @see glMapBufferRange
     * @see glUnmapBuffer
     */
    GLenum map_and_invoke(uint16_t idx, reader_callback_t callback, void* user_data) noexcept;
};
