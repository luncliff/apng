#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 i_position;
layout(location = 1) in vec3 i_color;

layout(location = 0) out vec3 v2f_color;

void entry1() {
    vec2 positions[3] = vec2[](vec2(0.0, -0.5), //
                               vec2(0.5, 0.5),  //
                               vec2(-0.5, 0.5)  //
    );
    vec3 colors[3] = vec3[](vec3(1, 0, 0), //
                            vec3(0, 1, 0), //
                            vec3(0, 0, 1)  //
    );
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
    v2f_color = colors[gl_VertexIndex];
}
void main() {
    gl_Position = vec4(i_position, 0, 1);
    v2f_color = i_color;
}
