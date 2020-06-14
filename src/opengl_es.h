/**
 * @see   https://github.com/google/angle 2019.12.31+
 */
#pragma once
#include "opengl.h"

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
// #include <EGL/eglplatform.h>
#if __has_include(<EGL/eglext_angle.h>)
#include <EGL/eglext_angle.h>
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
    ~egl_bundle_t() noexcept;
#if __has_include(<EGL/eglext_angle.h>)
    egl_bundle_t(EGLNativeWindowType native, //
                 bool is_console) noexcept(false);
#endif
};
