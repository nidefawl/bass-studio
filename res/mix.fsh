#version 150 core

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform float u_progress;

in vec2 pass_texcoord;

out vec4 out_Color;

void main(void) {
    vec4 colA = texture(tex1, pass_texcoord.st, 0);
    vec4 colB = texture(tex0, pass_texcoord.st, 0);
    out_Color = mix(colA, colB, u_progress);
    // out_Color = vec4(vec3(u_progress), 1.0);
}