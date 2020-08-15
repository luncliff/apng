#version 450

layout(location = 0) in vec3 v2f_color;

layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(v2f_color, 1);
}
