#include "opengl.h"

using namespace std;

opengl_vao_t::opengl_vao_t() noexcept {
    glGenVertexArrays(1, &name);
}
opengl_vao_t::~opengl_vao_t() noexcept {
    glDeleteVertexArrays(1, &name);
}

bool opengl_shader_program_t::get_shader_info(string& message, GLuint shader,
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

bool opengl_shader_program_t::get_program_info(string& message, GLuint program,
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

GLuint opengl_shader_program_t::create_compile_attach(
    GLuint program, GLenum shader_type, string_view code) noexcept(false) {
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

opengl_shader_program_t::opengl_shader_program_t(
    string_view vtxt, //
    string_view ftxt) noexcept(false)
    : id{glCreateProgram()}, vs{create_compile_attach(id, GL_VERTEX_SHADER,
                                                      vtxt)},
      fs{create_compile_attach(id, GL_FRAGMENT_SHADER, ftxt)} {
    glLinkProgram(id);
    string message{};
    if (get_program_info(message, id, GL_LINK_STATUS) == false)
        throw runtime_error{message};
}

opengl_shader_program_t::~opengl_shader_program_t() noexcept {
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteProgram(id);
}

opengl_shader_program_t::operator bool() const noexcept {
    return glIsProgram(id);
}

GLint opengl_shader_program_t::uniform(gsl::czstring<> name) const noexcept {
    return glGetUniformLocation(id, name);
}
GLint opengl_shader_program_t::attribute(gsl::czstring<> name) const noexcept {
    return glGetAttribLocation(id, name);
}

opengl_texture_t::operator bool() const noexcept {
    return glIsTexture(this->name);
}

opengl_texture_t::opengl_texture_t(GLuint _name, GLenum _target) noexcept(false)
    : name{_name}, target{_target} {
    if (glIsTexture(this->name) == false)
        throw invalid_argument{"not texture"};
}

opengl_texture_t::opengl_texture_t() noexcept(false)
    : name{},               // with the signature
      target{GL_TEXTURE_2D} // we can sure the type is GL_TEXTURE_2D
{
    glGenTextures(1, &name);
    if (int ec = glGetError())
        throw system_error{ec, get_opengl_category(), "glGenTextures"};
}

opengl_texture_t::~opengl_texture_t() noexcept(false) {
    glDeleteTextures(1, &name);
    if (int ec = glGetError())
        throw system_error{ec, get_opengl_category(), "glDeleteTextures"};
}

GLenum opengl_texture_t::update(uint16_t width, uint16_t height,
                                const void* ptr) noexcept {
    target = GL_TEXTURE_2D;
    glBindTexture(target, name);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    constexpr auto level = 0, border = 0;
    glTexImage2D(target, level,                  // no mipmap
                 GL_RGBA, width, height, border, //
                 GL_RGBA, GL_UNSIGNED_BYTE, ptr);
    const auto ec = glGetError();
    glBindTexture(target, 0);
    return ec;
}

opengl_framebuffer_t::opengl_framebuffer_t(uint16_t width,
                                           uint16_t height) noexcept(false) {
    if (width * height == 0)
        throw invalid_argument{"width * height == 0"};
    glGenFramebuffers(1, &name);
    glBindFramebuffer(GL_FRAMEBUFFER, name);
    glGenRenderbuffers(2, buffers);
    if (int ec = glGetError())
        throw system_error{ec, get_opengl_category(), "glGenRenderbuffers"};
    {
        glBindRenderbuffer(GL_RENDERBUFFER, buffers[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
        if (int ec = glGetError())
            throw system_error{ec, get_opengl_category(),
                               "glRenderbufferStorage"};
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, buffers[0]);
    }
    {
        glBindRenderbuffer(GL_RENDERBUFFER, buffers[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width,
                              height);
        if (int ec = glGetError())
            throw system_error{ec, get_opengl_category(),
                               "glRenderbufferStorage"};
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, buffers[1]);
    }
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

opengl_framebuffer_t::~opengl_framebuffer_t() noexcept {
    glDeleteRenderbuffers(2, buffers);
    glDeleteFramebuffers(1, &name);
}

GLenum opengl_framebuffer_t::bind(void*) noexcept {
    glBindFramebuffer(GL_FRAMEBUFFER, name);
    return glGetError();
}

GLenum opengl_framebuffer_t::read_pixels(uint16_t width, uint16_t height,
                                         void* pixels) noexcept {
    glBindFramebuffer(GL_FRAMEBUFFER, name);
    glReadPixels(0, 0, width, height, //
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return glGetError();
}

class opengl_error_category_t final : public error_category {
    const char* name() const noexcept override {
        return "OpenGL";
    }
    string message(int ec) const override {
        constexpr auto bufsz = 40;
        char buf[bufsz]{};
        const auto len = snprintf(buf, bufsz, "error %5d(%4x)", ec, ec);
        return {buf, static_cast<size_t>(len)};
    }
};

opengl_error_category_t cat{};
error_category& get_opengl_category() noexcept {
    return cat;
};
