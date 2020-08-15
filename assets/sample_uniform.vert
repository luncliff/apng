#version 450

layout(set = 0, binding = 0) uniform uniform0 {
    mat4 model;
    mat4 view;
    mat4 projection;
}
ubo; // uniform buffer object

layout(location = 0) in vec2 i_position;
layout(location = 1) in vec3 i_color;

layout(location = 0) out vec3 v2f_color;

void main() {
    mat4 mvp = ubo.projection * ubo.view * ubo.model;
    gl_Position = mvp * vec4(i_position, 0.0, 1.0);
    v2f_color = i_color;
}
