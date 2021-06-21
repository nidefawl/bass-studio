#version 150 core

uniform sampler2D tex0;
uniform float u_time;

in vec2 pass_texcoord;
out vec4 out_Color;

void main(void) {
    float red = texture(tex0, pass_texcoord.st, 0).r;
    vec4 color = vec4(red, red, red, 1.0);
    //  vec4 color = vec4(pass_texcoord.st, sin(u_time*0.001)*0.5+0.5, 1.0); 
     out_Color = color;
}