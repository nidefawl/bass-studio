#version 150 core

uniform mat4 u_mvp;

in vec2 a_position;
in vec2 a_texcoord;

out vec2 pass_texcoord;

void main(void) {
    pass_texcoord = a_texcoord.st;
    gl_Position = u_mvp * vec4(a_position.xy, 0.0, 1.0);
}