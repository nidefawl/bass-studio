#version 150 core

uniform sampler2D tex0;
uniform float u_time;
uniform float u_fade;

in vec2 pass_texcoord;
out vec4 out_Color;

void main(void) {
     vec2 vignetteLike = abs((pass_texcoord-0.5)*2.0)*0.9;
     float vignetteF = clamp((1.5-pow(1.1-(vignetteLike.x*vignetteLike.y), 2.0))*0.001+0.999, 0.0, 1.0);
     out_Color = vec4(vignetteF*u_fade);
}