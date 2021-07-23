#version 150 core

uniform sampler2D tex0;

in vec2 pass_texcoord;
out vec4 out_Color;

void main(void) {
     out_Color = texture(tex0, pass_texcoord);
}