#pragma once
#include "opengl.h"

struct texture2d_renderer_t final {
    opengl_shader_program_t program;
    opengl_vao_t vao;
    GLuint vbo, ebo;
    GLint u_mvp = -1;
    GLint a_position = -1, a_color = -1, a_texcoord = -1;

  public:
    texture2d_renderer_t() noexcept(false);
    ~texture2d_renderer_t() noexcept(false);

    GLenum render(void* context, GLuint texture,
                  GLenum target = GL_TEXTURE_2D) noexcept;

  private:
    GLenum bind(void* context) noexcept(false);
    GLenum unbind(void* context) noexcept;
};
