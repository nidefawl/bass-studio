#version 150 core

uniform mat4 u_mvp;

in vec2 a_position;

void main(void) {
    gl_Position = u_mvp * vec4(a_position.xy, 0.0, 1.0);
}