#version 150 core

uniform sampler2D tex0;
uniform float u_alpha;
uniform float u_time;
uniform float u_clock;


in vec2 pass_texcoord;

out vec4 out_Color;


//RADIUS of our vignette, where 0.5 results in a circle fitting the screen
const float RADIUS = 0.85;

//softness of our vignette, between 0.0 and 1.0
const float SOFTNESS = 0.65;

vec3 palette( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return clamp(a + b*cos( 6.28318*(c*t+d) ), vec3(0.0), vec3(1.0));
}
void main(void) {
	vec2 tc = pass_texcoord;
	vec3 bg = vec3(0.0, 0.05, 0.2)+0.3;
	bg = palette(sin(u_clock*0.2)*0.5+0.5, vec3(0.1), vec3(0.1), vec3(0.5), vec3(0.0, 0.33, 0.66));
	vec3 barColor = vec3(0.9);
	float f1 = 0.4;
	float gradient = smoothstep(0.0, f1, pass_texcoord.y)-smoothstep(1.0-f1, 1.0, pass_texcoord.y);


	//1. VIGNETTE
	
	//determine center position
	vec2 position = (pass_texcoord.xy) - vec2(0.5);
	position.x*=0.7;
	//determine the vector length of the center position
	float len = length(position);
	
	//use smoothstep to create a smooth vignette
	float vignette = smoothstep(RADIUS, RADIUS-SOFTNESS, len);
	vec3 colOut = mix(vec3(bg*0.7), vec3(bg), gradient);
	colOut = mix(colOut, vec3(colOut*1.4), vignette);
	// colOut = vec3(gradient*vignette);
	if (tc.y>0.5)
		colOut.rgb += texture(tex0, pass_texcoord.st, 0).rgb*barColor;
	else {
		vec2 posReflect = vec2(tc.x, 1.0-tc.y);
		float f = 1.0-((posReflect.y-0.5)/0.5);
		f = pow(f, 2.0)*0.122+0.01;
		vec4 colReflect = texture(tex0, posReflect, 0);
		colReflect.rgb *= f;
		colOut.rgb += colReflect.rgb*barColor;
		// out_Color = vec4(vec3(f), 1.0);
	}
	out_Color = vec4(colOut, 1);
}