/**
 * @author Park DongHa (luncliff@gmail.com)
 */
#pragma once
// clang-format off
#if defined(FORCE_STATIC_LINK)
#   define _INTERFACE_
#   define _HIDDEN_
#elif defined(_MSC_VER) // MSVC or clang-cl
#   define _HIDDEN_
#   ifdef _WINDLL
#       define _INTERFACE_ __declspec(dllexport)
#   else
#       define _INTERFACE_ __declspec(dllimport)
#   endif
#elif defined(__GNUC__) || defined(__clang__)
#   define _INTERFACE_ __attribute__((visibility("default")))
#   define _HIDDEN_ __attribute__((visibility("hidden")))
#else
#   error "unexpected linking configuration"
#endif
// clang-format on
#include <filesystem>
#include <gsl/gsl>
#include <memory_resource>
#include <string_view>
#include <system_error>
// clang-format off
#if __has_include(<d3d11.h>) // Windows, DirectX 11
//#include <winrt/base.h>
#include <d3d11.h>
#endif
#if __has_include(<vulkan/vulkan.h>) // Vulkan API
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
_INTERFACE_ std::error_category& get_opengl_category() noexcept;
_INTERFACE_ void get_extensions(EGLDisplay display, std::vector<std::string_view>& names) noexcept;
_INTERFACE_ bool has_extension(EGLDisplay display, std::string_view name) noexcept;

/**
 * @brief `EGLSurface` owner.
 * @todo Bind/unbind with `EGLNativeWindowType` using `resume`/`suspend` 
 */
class _INTERFACE_ egl_surface_owner_t final {
    EGLDisplay display;
    EGLConfig config;
    gsl::owner<EGLSurface> surface = EGL_NO_SURFACE;

  public:
    egl_surface_owner_t(EGLDisplay display, EGLConfig config, EGLSurface surface) noexcept;
    ~egl_surface_owner_t() noexcept;
    egl_surface_owner_t(egl_surface_owner_t const&) = delete;
    egl_surface_owner_t& operator=(egl_surface_owner_t const&) = delete;
    egl_surface_owner_t(egl_surface_owner_t&&) = delete;
    egl_surface_owner_t& operator=(egl_surface_owner_t&&) = delete;

    EGLint get_size(EGLint& width, EGLint& height) noexcept;
    EGLSurface handle() const noexcept;
};

/**
 * @brief `EGLContext` and `EGLConfig` owner.
 * @see   https://www.saschawillems.de/blog/2015/04/19/using-opengl-es-on-windows-desktops-via-egl/
 */
class _INTERFACE_ egl_context_t final {
  private:
    EGLDisplay display;
    EGLint versions[2]{};   // major, minor
    EGLConfig configs[1]{}; // ES 2.0, Window/Pbuffer, RGBA 32, Depth 16
    gsl::owner<EGLContext> context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;

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
     * @brief Destroy all EGL bindings and resources
     * @note This functions in invoked in the destructor
     * @post is_valid() == false
     * 
     * @see eglMakeCurrent
     * @see eglDestroyContext
     * @see eglDestroySurface
     * @see eglTerminate(unused) https://www.khronos.org/registry/EGL/sdk/docs/man/html/eglTerminate.xhtml
     */
    void destroy() noexcept;

    /**
     * @brief   Create EGLSurface with given EGLNativeWindowType and bind with EGLContext
     * @return  EGLint  Redirected from `eglGetError`. 
     *                  `EGL_NOT_INITIALIZED` if `terminate` is invoked.
     * @see eglCreateWindowSurface
     * @see eglQuerySurface
     * @see eglMakeCurrent
     * @todo    https://github.com/google/angle/blob/master/extensions/EGL_ANGLE_direct_composition.txt
     * @todo    support eglCreatePlatformWindowSurface?
     */
    [[deprecated]] EGLint resume(gsl::not_null<EGLNativeWindowType> window) noexcept;

    /**
     * @brief   Take ownership of the given EGLSurface
     * 
     * @param es_surface  Expect PBufferSurface.
     * @param es_config   Hint to prevent misuse of `resume(EGLNativeWindowType)`. 
     *                    Always ignored. 
     */
    EGLint resume(EGLSurface es_surface, [[maybu_unused]] EGLConfig es_config) noexcept;

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

    /**
     * @brief EGLContext == NULL?
     * @details It is recommended to invoke this function to check whether the construction was successful.
     *          Notice that the constructor is `noexcept`.
     */
    EGLContext handle() const noexcept;
    EGLConfig config() const noexcept;
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
 * @see GL_PIXEL_PACK_BUFFER
 * @see GL_EXT_map_buffer_range https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_map_buffer_range.txt
 */
class _INTERFACE_ pbo_reader_t final {
    static constexpr uint16_t capacity = 2;

  private:
    GLuint pbos[capacity];
    uint32_t length; // byte length of the buffer modification
    GLintptr offset;
    GLenum ec = GL_NO_ERROR;

  public:
    explicit pbo_reader_t(GLuint length) noexcept;
    ~pbo_reader_t() noexcept;
    pbo_reader_t(pbo_reader_t const&) = delete;
    pbo_reader_t& operator=(pbo_reader_t const&) = delete;
    pbo_reader_t(pbo_reader_t&&) = delete;
    pbo_reader_t& operator=(pbo_reader_t&&) = delete;

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

/// @see memcpy
using writer_callback_t = void (*)(void* user_data, void* mapping, size_t length);

/**
 * @see GL_PIXEL_UNPACK_BUFFER
 * @see GL_EXT_map_buffer_range https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_map_buffer_range.txt
 */
class _INTERFACE_ pbo_writer_t final {
    static constexpr uint16_t capacity = 2;

  private:
    GLuint pbos[capacity];
    uint32_t length;
    GLenum ec = GL_NO_ERROR;

  public:
    explicit pbo_writer_t(GLuint length) noexcept;
    ~pbo_writer_t() noexcept;
    pbo_writer_t(pbo_writer_t const&) = delete;
    pbo_writer_t& operator=(pbo_writer_t const&) = delete;
    pbo_writer_t(pbo_writer_t&&) = delete;
    pbo_writer_t& operator=(pbo_writer_t&&) = delete;

    /**
     * @brief check whether the construction was successful
     * @return GLenum   cached `ec` from the constructor
     */
    GLenum is_valid() const noexcept;

    /// @see GL_TEXTURE_2D
    GLenum unpack(uint16_t idx, GLuint tex2d, const GLint frame[4], //
                  GLenum format = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE) noexcept;
    GLenum map_and_invoke(uint16_t idx, writer_callback_t callback, void* user_data) noexcept;
};

#if __has_include(<d3d11.h>)

/**
 * @todo    Give more clear relation between `EGLConfig` and `ID3D11Texture2D`
 * @param texture   DirectX texture to bind with EGLSurface(EGL_TEXTURE_2D,EGL_TEXTURE_RGBA)
 * @param surface   Client pixel buffer surface
 * @return uint32_t 0 if successful
 *                  `ENOTSUP` if EGL extension is not available.
 *                  `HRESULT` if DirectX reports error
 *                  `EGLint` from `eglGetError`
 */
_INTERFACE_ uint32_t make_egl_client_surface(EGLDisplay display, EGLConfig config, //
                                             ID3D11Texture2D* texture, EGLSurface& surface) noexcept;

#endif
