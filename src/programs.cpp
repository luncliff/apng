
#include "programs.h"

using namespace std;

texture2d_renderer_t::texture2d_renderer_t() noexcept(false)
    : program{R"(
#version 100 // ES 2.0
uniform mat4 u_mvp;
attribute vec4 a_position;
attribute vec2 a_texcoord;
attribute vec3 a_color;
varying vec3 v_color;
varying vec2 v_texcoord;
void main() {
    gl_Position = u_mvp * a_position;
    v_color = a_color;
    v_texcoord = a_texcoord;
})",
              R"(
#version 100 // ES 2.0
precision mediump float;
uniform sampler2D u_tex2d;
varying vec3 v_color;
varying vec2 v_texcoord;
void main() {
    gl_FragColor = texture2D(u_tex2d, v_texcoord) * vec4(v_color, 1);
})"},
      vao{} {
    glBindVertexArray(vao.name);
    if (int ec = glGetError())
        throw system_error{ec, get_opengl_category(), "glBindVertexArray"};

    constexpr float ratio = 0.93f, z0 = 0; // nearly full-size
    constexpr float tx0 = 0, ty0 = 0, tx = 1, ty = 1;
    const GLfloat vertices[32] = {
        // position(3), color(3), texture coord(2)
        ratio,  ratio,  z0, 1, 1, 1, tx,  ty,  // top right
        ratio,  -ratio, z0, 1, 1, 1, tx,  ty0, // bottom right
        -ratio, -ratio, z0, 1, 1, 1, tx0, ty0, // bottom left
        -ratio, ratio,  z0, 1, 1, 1, tx0, ty   // top left
    };
    glGenBuffers(1, &vbo);
    if (int ec = glGetError())
        throw system_error{ec, get_opengl_category(), "glGenBuffers"};

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    if (int ec = glGetError())
        throw system_error{ec, get_opengl_category(), "glBufferData"};
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const GLuint indices[6] = {
        0, 1, 3, // up-left
        1, 2, 3  // bottom-right
    };
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);
    if (int ec = glGetError())
        throw system_error{ec, get_opengl_category(), "glBufferData"};
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

texture2d_renderer_t::~texture2d_renderer_t() noexcept(false) {
    GLuint buffers[2] = {vbo, ebo};
    glDeleteBuffers(2, buffers);
    if (int ec = glGetError())
        throw system_error{ec, get_opengl_category(), "glDeleteBuffers"};
}

GLenum texture2d_renderer_t::unbind(void*) noexcept {
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return glGetError();
}

GLenum texture2d_renderer_t::bind(void*) noexcept(false) {
    glUseProgram(program.id);
    if (int ec = glGetError())
        throw system_error{ec, get_opengl_category(), "glUseProgram"};

    glBindVertexArray(vao.name);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    if (u_mvp == -1) {
        u_mvp = program.uniform("u_mvp");
        const float mvp[16] = {
            1, 0, 0, 0, // identity
            0, 1, 0, 0, //
            0, 0, 1, 0, //
            0, 0, 0, 1, //
        };
        glUniformMatrix4fv(u_mvp, 1, GL_FALSE, &mvp[0]);
        if (int ec = glGetError())
            throw system_error{ec, get_opengl_category(), "glUniformMatrix4fv"};
    }
    if (a_position == -1) {
        a_position = program.attribute("a_position");
        glVertexAttribPointer(a_position, 3, GL_FLOAT, //
                              GL_FALSE, 8 * sizeof(float), (void*)0);
        if (int ec = glGetError())
            throw system_error{ec, get_opengl_category(),
                               "glVertexAttribPointer"};
        glEnableVertexAttribArray(a_position);
    }
    if (a_color == -1) {
        a_color = program.attribute("a_color");
        glVertexAttribPointer(a_color, 3, GL_FLOAT, //
                              GL_FALSE, 8 * sizeof(float),
                              (void*)(3 * sizeof(float)));
        if (int ec = glGetError())
            throw system_error{ec, get_opengl_category(),
                               "glVertexAttribPointer"};
        glEnableVertexAttribArray(a_color);
    }
    if (a_texcoord == -1) {
        a_texcoord = program.attribute("a_texcoord");
        glVertexAttribPointer(a_texcoord, 2, GL_FLOAT, //
                              GL_FALSE, 8 * sizeof(float),
                              (void*)(6 * sizeof(float)));
        if (int ec = glGetError())
            throw system_error{ec, get_opengl_category(),
                               "glVertexAttribPointer"};
        glEnableVertexAttribArray(a_texcoord);
    }
    return glGetError();
}

GLenum texture2d_renderer_t::render(void* context, GLuint texture,
                                    GLenum target) noexcept {
    if (auto ec = this->bind(context))
        return ec;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(target, texture);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    if (auto ec = glGetError())
        return ec;
    glBindTexture(target, 0);
    return this->unbind(context);
}
