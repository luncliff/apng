#include "opengl.h"

auto make_offscreen_context(uint32_t& ec, std::string& message) noexcept
    -> std::shared_ptr<void> {
    CGLPixelFormatAttribute attrs[] = {
        // kCGLPFAOffScreen, // bad attribute
        kCGLPFAAccelerated,
        kCGLPFAOpenGLProfile,
        (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
        kCGLPFAColorSize,
        (CGLPixelFormatAttribute)32,
        (CGLPixelFormatAttribute)NULL};
    CGLPixelFormatObj pixel_format{};
    GLint num_pixel_format{};
    if ((ec = CGLChoosePixelFormat(attrs, &pixel_format, &num_pixel_format))) {
        message = "CGLChoosePixelFormat";
        return nullptr;
    }
    CGLContextObj context{};
    if ((ec = CGLCreateContext(pixel_format, NULL, &context))) {
        message = "CGLCreateContext";
        return nullptr;
    }
    if ((ec = CGLDestroyPixelFormat(pixel_format))) {
        message = "CGLDestroyPixelFormat";
        return nullptr;
    }
    if ((ec = CGLSetCurrentContext(context))) {
        message = "CGLSetCurrentContext";
        return nullptr;
    }
    message.clear();
    return std::shared_ptr<void>{context, [context](void*) {
                                     CGLSetCurrentContext(NULL);
                                     CGLClearDrawable(context);
                                     CGLDestroyContext(context);
                                 }};
}
