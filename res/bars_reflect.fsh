#version 150 core

uniform float u_numbands;
uniform float u_time;
uniform float u_clock;
uniform vec2 u_viewport;
uniform sampler2D tex0;
uniform sampler2D tex1;


in vec2 pass_texcoord;
in vec4 pass_color;
in vec4 pass_attr;
in vec4 pass_attr2;
flat in vec2 barSize;
 
out vec4 out_Color;
#define attr pass_attr
vec3 palette( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b*cos( 6.28318*(c*t+d) );
}
float sdRoundBox( in vec2 p, in vec2 b) 
{
    vec2 q = abs(p) - b;
    return min(max(q.x,q.y),0.0) + length(max(q,0.0));
}

vec4 rectBars(void) {
	return vec4(vec3(1), 1);
}
vec4 roundBars(void) {
	float a;
	if (pass_texcoord.y < 0.5) {
		a = length(pass_texcoord.xy - vec2(0.5));
	} else {
		a = abs(pass_texcoord.x-0.5);
	}
	a *= 1.05;
	a = 1.0 - a;
	a -= 0.5;
	a *= 4;
	a = clamp(a, 0.0, 1.0);
	float inverseTexcoordY = pass_attr.x-1.0;
	vec3 vc = vec3(0.4, 0.42, 0.46);
	float band = pass_attr.y/u_numbands;
	float lvl = pass_attr.z;
	lvl = 1.0-lvl;
	lvl = pow(lvl, 8.0);
	lvl = 1.0-lvl;
	float param = 0.0;
	param = pass_texcoord.y/16.0;
	param = (pass_texcoord.y/4.0*lvl);
	vec3 col = palette( lvl*1.4+0.2, vec3(0.5),vc,vec3(0.5), vec3(0.5));
	vec3 color = vec3(1.0);
	return vec4(color, a);
}

void main(void) {
	vec4 color = roundBars();
	out_Color = color;
}