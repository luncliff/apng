#include "gles_with_angle.h"

#include <cassert>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;

class gles_error_t : public error_category {
    const char* name() const noexcept override {
        return "ANGLE";
    }
    string message(int ec = glGetError()) const override {
        constexpr auto bufsz = 40;
        char buf[bufsz]{};
        auto len = snprintf(buf, bufsz, "error %d(%4x)", ec, ec);
        return {buf, static_cast<size_t>(len)};
    }
};

gles_error_t gles_category{};

error_category& get_gles_category() noexcept {
    return gles_category;
};

vao_t::vao_t() noexcept {
    glGenVertexArrays(1, &name);
}
vao_t::~vao_t() noexcept {
    glDeleteVertexArrays(1, &name);
}

bool get_shader_info(string& message, GLuint shader,
                     GLenum status_name) noexcept {
    GLint info = GL_FALSE;
    glGetShaderiv(shader, status_name, &info);
    if (info != GL_TRUE) {
        GLsizei buf_len = 400;
        message.resize(buf_len);
        glGetShaderInfoLog(shader, buf_len, &buf_len, message.data());
        message.resize(buf_len); // shrink
    }
    return info;
}

bool get_program_info(string& message, GLuint program,
                      GLenum status_name) noexcept {
    GLint info = GL_TRUE;
    glGetProgramiv(program, status_name, &info);
    if (info != GL_TRUE) {
        GLsizei buf_len = 400;
        message.resize(buf_len);
        glGetProgramInfoLog(program, buf_len, &buf_len, message.data());
        message.resize(buf_len); // shrink
    }
    return info;
}

GLuint create_compile_attach(GLuint program, GLenum shader_type,
                             string_view code) noexcept(false) {
    auto shader = glCreateShader(shader_type);
    const GLchar* begin = code.data();
    const GLint len = code.length();
    glShaderSource(shader, 1, &begin, &len);
    glCompileShader(shader);
    string message{};
    if (get_shader_info(message, shader, GL_COMPILE_STATUS) == false)
        throw runtime_error{message};

    glAttachShader(program, shader);
    return shader;
}

program_t::program_t(string_view vtxt, //
                     string_view ftxt) noexcept(false)
    : id{glCreateProgram()}, //
      vs{create_compile_attach(id, GL_VERTEX_SHADER, vtxt)},
      fs{create_compile_attach(id, GL_FRAGMENT_SHADER, ftxt)} {
    glLinkProgram(id);
    string message{};
    if (get_program_info(message, id, GL_LINK_STATUS) == false)
        throw runtime_error{message};
}

program_t::~program_t() noexcept {
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteProgram(id);
}

program_t::operator bool() const noexcept {
    return glIsProgram(id);
}

GLint program_t::uniform(gsl::czstring<> name) const noexcept {
    return glGetUniformLocation(id, name);
}
GLint program_t::attribute(gsl::czstring<> name) const noexcept {
    return glGetAttribLocation(id, name);
}

texture_t::texture_t(GLuint _name, GLenum _target) noexcept(false)
    : name{_name}, target{_target} {
    if (glIsTexture(_name) == false)
        throw invalid_argument{"not texture"};
}

texture_t::texture_t(uint16_t width, uint16_t height,
                     uint32_t* ptr) noexcept(false)
    : name{}, target{GL_TEXTURE_2D} // we can sure the type is GL_TEXTURE_2D
{
    glGenTextures(1, &name);
    if (int ec = glGetError())
        throw system_error{ec, get_gles_category(), "glGenTextures"};
    if (int ec = update(width, height, ptr))
        throw system_error{ec, get_gles_category(), "glTexImage2D"};
}

texture_t::~texture_t() noexcept(false) {
    glDeleteTextures(1, &name);
    if (int ec = glGetError())
        throw system_error{ec, get_gles_category(), "glDeleteTextures"};
}

GLenum texture_t::update(uint16_t width, uint16_t height,
                         uint32_t* ptr) noexcept {
    auto on_return = gsl::finally([target = this->target]() {
        glBindTexture(target, 0); //
    });
    glBindTexture(target, name);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    constexpr auto level = 0, border = 0;
    glTexImage2D(target, level,                  // no mipmap
                 GL_RGBA, width, height, border, //
                 GL_RGBA, GL_UNSIGNED_BYTE, ptr);
    return glGetError();
}

framebuffer_t::framebuffer_t(uint16_t width, uint16_t height) noexcept(false) {
    glGenFramebuffers(1, &name);
    glBindFramebuffer(GL_FRAMEBUFFER, name);

    glGenRenderbuffers(2, buffers);

    glBindRenderbuffer(GL_RENDERBUFFER, buffers[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    if (auto ec = glGetError())
        throw runtime_error{
            "glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, ...)"};
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, buffers[0]);

    glBindRenderbuffer(GL_RENDERBUFFER, buffers[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    if (auto ec = glGetError())
        throw runtime_error{"glRenderbufferStorage(GL_RENDERBUFFER, "
                            "GL_DEPTH_COMPONENT16, ...)"};
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, buffers[1]);

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

framebuffer_t::~framebuffer_t() noexcept {
    glDeleteRenderbuffers(2, buffers);
    glDeleteFramebuffers(1, &name);
}

GLenum framebuffer_t::bind() noexcept {
    glBindFramebuffer(GL_FRAMEBUFFER, name);
    //glBindRenderbuffer(GL_RENDERBUFFER, buffers[0]);
    return glGetError();
}

egl_bundle_t::egl_bundle_t(EGLNativeDisplayType native) noexcept(false)
    : native_window{}, native_display{native} {
    display = eglGetDisplay(native_display);
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglGetDisplay(EGL_DEFAULT_DISPLAY)"};
    }
    eglInitialize(display, &major, &minor);
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglInitialize"};
    }
    EGLint count = 0;
    eglChooseConfig(display, nullptr, &config, 1, &count);
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglChooseConfig"};
    }
    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
        throw runtime_error{"eglBindAPI(EGL_OPENGL_ES_API)"};
    }
    const EGLint attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglCreateContext"};
    }
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
    if (eglGetError() != EGL_SUCCESS) {
        throw runtime_error{"eglMakeCurrent"};
    }
}

#if defined(_WIN32)
egl_bundle_t::~egl_bundle_t() {
    // Win32
    if (display)
        ReleaseDC(native_window, native_display);
    // EGL
    if (surface != EGL_NO_SURFACE)
        eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT)
        eglDestroyContext(display, context);
    if (display != EGL_NO_DISPLAY)
        eglTerminate(display);
}
#else
egl_bundle_t::~egl_bundle_t() {
    // EGL
    if (surface != EGL_NO_SURFACE)
        eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT)
        eglDestroyContext(display, context);
    if (display != EGL_NO_DISPLAY)
        eglTerminate(display);
}
#endif

#if __has_include("EGL/eglext_angle.h")
egl_bundle_t::egl_bundle_t(EGLNativeWindowType native,
                           bool is_console) noexcept(false)
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
        EGLint num_attrs = 0;
        if (eglChooseConfig(display, attrs, &config, 1, &num_attrs) != EGL_TRUE)
            throw runtime_error{"eglChooseConfig"};
    }
    if (is_console == false) {
        const EGLint attrs[] = {EGL_DIRECT_COMPOSITION_ANGLE, EGL_TRUE,
                                EGL_NONE};
        // Create window surface
        surface = eglCreateWindowSurface(display, config, native_window, attrs);
        if (surface == nullptr)
            throw runtime_error{"eglCreateWindowSurface"};
    }
    // Create EGL context
    {
        const EGLint attrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrs);
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
#endif

tex2d_renderer_t::tex2d_renderer_t() noexcept(false)
    : vao{}, program{R"(
uniform mat4 u_mvp;
attribute vec4 a_position;
attribute vec2 a_texcoord;
attribute vec3 a_color;
varying vec3 v_color;
varying vec2 v_texcoord;
void main()
{
    gl_Position = u_mvp * a_position;
    v_color = a_color;
    v_texcoord = a_texcoord;
})",
                     R"(
precision mediump float;
uniform sampler2D u_tex2d;
varying vec3 v_color;
varying vec2 v_texcoord;
void main()
{
    gl_FragColor = texture2D(u_tex2d, v_texcoord) * vec4(v_color, 1);
})"} {
    glBindVertexArray(vao.name);
    assert(glGetError() == GL_NO_ERROR);

    constexpr float ratio = 0.95f, z0 = 0; // nearly full-size
    constexpr float tx0 = 0, ty0 = 0, tx = 1, ty = 1;
    const GLfloat vertices[32] = {
        // position(3), color(3), texture coord(2)
        ratio,  ratio,  z0, 1, 1, 1, tx,  ty,  // top right
        ratio,  -ratio, z0, 1, 1, 1, tx,  ty0, // bottom right
        -ratio, -ratio, z0, 1, 1, 1, tx0, ty0, // bottom left
        -ratio, ratio,  z0, 1, 1, 1, tx0, ty   // top left
    };
    glGenBuffers(1, &vbo);
    assert(glGetError() == GL_NO_ERROR);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    assert(glGetError() == GL_NO_ERROR);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const GLuint indices[6] = {
        0, 1, 3, // up-left
        1, 2, 3  // bottom-right
    };
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);
    assert(glGetError() == GL_NO_ERROR);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

tex2d_renderer_t::~tex2d_renderer_t() noexcept(false) {
    GLuint buffers[2] = {vbo, ebo};
    glDeleteBuffers(2, buffers);
    assert(glGetError() == GL_NO_ERROR);
}

GLenum tex2d_renderer_t::unbind(EGLContext) noexcept {
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return glGetError();
}

GLenum tex2d_renderer_t::bind(EGLContext) noexcept {
    glUseProgram(program.id);
    if (auto ec = glGetError())
        return ec;

    glBindVertexArray(vao.name);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    {
        auto idx = program.uniform("u_mvp");
        auto mvp = glm::identity<glm::mat4>();
        glUniformMatrix4fv(idx, 1, GL_FALSE, &mvp[0][0]);
        if (auto ec = glGetError())
            return ec;
    }
    {
        auto idx = program.attribute("a_position");
        glVertexAttribPointer(idx, 3, GL_FLOAT, //
                              GL_FALSE, 8 * sizeof(float), (void*)0);
        if (auto ec = glGetError())
            return ec;
        glEnableVertexAttribArray(idx);
    }
    {
        auto idx = program.attribute("a_color");
        glVertexAttribPointer(idx, 3, GL_FLOAT, //
                              GL_FALSE, 8 * sizeof(float),
                              (void*)(3 * sizeof(float)));
        if (auto ec = glGetError())
            return ec;
        glEnableVertexAttribArray(idx);
    }
    {
        auto idx = program.attribute("a_texcoord");
        glVertexAttribPointer(idx, 2, GL_FLOAT, //
                              GL_FALSE, 8 * sizeof(float),
                              (void*)(6 * sizeof(float)));
        if (auto ec = glGetError())
            return ec;
        glEnableVertexAttribArray(idx);
    }
    return glGetError();
}

GLenum tex2d_renderer_t::render(EGLContext context, //
                                GLuint texture, GLenum target) noexcept {
    if (auto ec = bind(context))
        return ec;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(target, texture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    if (auto ec = glGetError())
        return ec;
    glBindTexture(target, 0);
    return this->unbind(context);
}
