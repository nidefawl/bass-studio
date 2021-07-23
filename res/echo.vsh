#version 150 core

uniform float u_time;
uniform mat4 u_mvp;
uniform vec4 params;

in vec2 a_position;
in vec2 a_texcoord;

out vec2 pass_texcoord;

void main(void) {
    pass_texcoord = a_texcoord.st;
    float p = u_time*0.3+params.x*10;
    float f1 = sin(p);
    float f2 = cos(p);
    vec3 pos = vec3(a_position.xy, 0);
    gl_Position = u_mvp * vec4(pos, 1.0);
}