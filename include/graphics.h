/**
 * @file graphics.h
 * @author github.com/luncliff (luncliff@gmail.com)
 */
#pragma once
#include <gsl/gsl>
#include <winrt/Windows.Foundation.h>

#include <filesystem>
#include <memory_resource>
#include <string_view>
#include <system_error>

#if __has_include(<vulkan/vulkan.h>)
#include <vulkan/vulkan.h>
#endif

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
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

/**
 * @brief `std::error_category` for `std::system_error` in this module
 * @see   `std::system_error`
 * @see   `std::error_category`
 */
std::error_category& get_opengl_category() noexcept;

class context_t final {
  private:
    gsl::owner<EGLDisplay> display = EGL_NO_DISPLAY;
    uint16_t major = 0, minor = 0;
    gsl::owner<EGLContext> context = EGL_NO_CONTEXT;
    EGLConfig configs[3]{};
    gsl::owner<EGLSurface> surface = EGL_NO_SURFACE; // EGLSurface for Draw/Read
    int32_t surface_width = 0;
    int32_t surface_height = 0;

  public:
    /**
     * @brief Acquire EGLDisplay and create an EGLContext for OpenGL ES 3.0+
     * @see eglInitialize
     * @see eglChooseConfig
     * @see eglCreateContext
     */
    explicit context_t(EGLContext share_context) noexcept;
    /**
     * @see terminate
     */
    ~context_t() noexcept;
    context_t(context_t const&) = delete;
    context_t& operator=(context_t const&) = delete;
    context_t(context_t&&) = delete;
    context_t& operator=(context_t&&) = delete;

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
