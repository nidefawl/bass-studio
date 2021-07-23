#version 150 core

uniform sampler2D tex0;
uniform float u_alpha;


in vec2 pass_texcoord;

out vec4 out_Color;

void main(void) {
    out_Color = texture(tex0, pass_texcoord.st, 0)*u_alpha;
}