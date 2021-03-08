#include <graphics.h>

GLuint create_compile_attach(GLuint program, GLenum shader_type, std::string_view code) noexcept(false);
bool get_shader_info(std::string& message, GLuint shader, GLenum status_name = GL_COMPILE_STATUS) noexcept;
bool get_program_info(std::string& message, GLuint program, GLenum status_name = GL_LINK_STATUS) noexcept;

bool get_shader_info(std::string& message, GLuint shader, GLenum status_name) noexcept {
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

bool get_program_info(std::string& message, GLuint program, GLenum status_name) noexcept {
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

GLuint create_compile_attach(GLuint program, GLenum shader_type, std::string_view code) noexcept(false) {
    auto shader = glCreateShader(shader_type);
    const GLchar* begin = code.data();
    const GLint len = static_cast<GLint>(code.length());
    glShaderSource(shader, 1, &begin, &len);
    glCompileShader(shader);
    std::string message{};
    if (get_shader_info(message, shader, GL_COMPILE_STATUS) == false)
        throw std::runtime_error{message};
    glAttachShader(program, shader);
    return shader;
}
