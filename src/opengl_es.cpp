#include <opengl_1.h>
#if __has_include(<EGL/eglext_angle.h>)
#include <EGL/eglext_angle.h>
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;

egl_helper_t::egl_helper_t(EGLNativeDisplayType native) noexcept(false)
    : native_window{}, native_display{native}, display{eglGetDisplay(native_display)} {
    ;
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglGetDisplay(EGL_DEFAULT_DISPLAY)"};
    }
    eglInitialize(display, &major, &minor);
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglInitialize"};
    }
    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
        throw runtime_error{"eglBindAPI(EGL_OPENGL_ES_API)"};
    }
    eglChooseConfig(display, nullptr, configs, 10, &count);
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglChooseConfig"};
    }
    const EGLint attrs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
    context = eglCreateContext(display, configs[0], EGL_NO_CONTEXT, attrs);
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglCreateContext"};
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglMakeCurrent"};
    }
}

#if defined(_WIN32)

egl_helper_t::~egl_helper_t() noexcept {
    // Win32
    if (display)
        ReleaseDC(native_window, native_display);
    // EGL
    if (context != EGL_NO_CONTEXT) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
    }
    if (surface != EGL_NO_SURFACE)
        eglDestroySurface(display, surface);
    if (display != EGL_NO_DISPLAY)
        eglTerminate(display);
}

egl_helper_t::egl_helper_t(EGLNativeWindowType native, bool is_console) noexcept(false)
    : native_window{native}, native_display{GetDC(native)} {
    {
        const EGLint attrs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                                EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                                EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE,
                                EGL_DONT_CARE,
                                EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE,
                                EGL_DONT_CARE,
                                EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                                EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE,
                                EGL_NONE};
        display = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, //
                                           native_display, attrs);
        if (eglInitialize(display, &major, &minor) != EGL_TRUE)
            throw runtime_error{"eglGetPlatformDisplayEXT, eglInitialize"};
    }
    {
        eglBindAPI(EGL_OPENGL_ES_API);
        if (eglGetError() != EGL_SUCCESS)
            throw runtime_error{"eglBindAPI(EGL_OPENGL_ES_API)"};
    }
    // Choose a config
    {
        const EGLint attrs[] = {EGL_NONE};
        if (eglChooseConfig(display, attrs, configs, 10, &count) != EGL_TRUE)
            throw runtime_error{"eglChooseConfig"};
    }
    if (is_console == false) {
        const EGLint attrs[] = {EGL_DIRECT_COMPOSITION_ANGLE, EGL_TRUE, EGL_NONE};
        // Create window surface
        surface = eglCreateWindowSurface(display, configs[0], native_window, attrs);
        if (surface == nullptr)
            throw runtime_error{"eglCreateWindowSurface"};
    }
    // Create EGL context
    {
        const EGLint attrs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
        context = eglCreateContext(display, configs[0], EGL_NO_CONTEXT, attrs);
        if (eglGetError() != EGL_SUCCESS)
            throw runtime_error{"eglCreateContext"};
    }
    // Make the surface current
    {
        eglMakeCurrent(display, surface, surface, context);
        if (eglGetError() != EGL_SUCCESS)
            throw runtime_error{"eglMakeCurrent"};
    }
}
#else
egl_helper_t::~egl_helper_t() noexcept {
    // EGL
    if (surface != EGL_NO_SURFACE)
        eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT)
        eglDestroyContext(display, context);
    if (display != EGL_NO_DISPLAY)
        eglTerminate(display);
}
#endif
