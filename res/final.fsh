#version 150 core

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec2 u_viewport;
uniform float u_time;
uniform float u_clock;


in vec2 pass_texcoord;

out vec4 out_Color;

void srgb(inout float v)
{
    float gamma = 2.4;
    v = pow(v, 1.0 / gamma);
}
void linToSrgb(inout vec3 linear) {
    srgb(linear.x);
    srgb(linear.y);
    srgb(linear.z);
}
vec3 sampleShifted(sampler2D tex, vec2 tc, vec2 offset, vec2 offset1, vec2 offset2) {
	vec3 s1 = texture(tex, tc+offset, 0).rgb;
	vec3 s2 = texture(tex, tc+offset1, 0).rgb;
	vec3 s3 = texture(tex, tc+offset2, 0).rgb;
	return vec3(s1.r, s2.g, s3.b);
}
vec3 doWhatever(vec2 mouse, vec2 tc) {
    vec2 p = -1.0 + 2.0 * tc;
    vec2 m = mouse;
    float a1 = atan(p.y-m.y,p.x-m.x);
    float r1 = sqrt(dot(p-m,p-m));
    float a2 = atan(p.y+m.y,p.x+m.x);
    float r2 = sqrt(dot(p+m,p+m));
    vec2 uv;
    uv.x = abs(r1-r2)*0.25;
    uv.y = sin(0.5*(a1-a2))+1;

    uv = mod(uv, vec2(1.0));
    uv = vec2(abs(uv.x-0.5)*2, abs(uv.y-0.5)*2);
    return vec3(uv, 1);
}
void main(void) {
	vec2 texDim = textureSize(tex0, 0);
	float step = 1.0f/texDim.x;
	vec4 sampleA = texture(tex0, pass_texcoord.st, 0);
	sampleA.rgb = sampleShifted(tex0, pass_texcoord.st, vec2(step*2, 0), vec2(0, step*0), vec2(step*-2, 0));
	vec3 colorA = sampleA.rgb;
	// vec3 colorB = area();
	vec3 colorB = texture(tex1, pass_texcoord.st, 0).rgb*0.1;
    vec3 final = colorA+colorB;
    // final = pow(final, vec3(1.9));
    // linToSrgb(final);
 //    out_Color = vec4(final, 1.0);

    // vec2 uv = pass_texcoord;
    // vec2 tc = vec2(abs(pass_texcoord.x-0.5)*2, pass_texcoord.y);
    // float a = sin(u_clock*0.33);
    // float b = cos(u_clock*0.35+4);
    // vec2 mouse = vec2(a, b);
    // vec3 uv2 = doWhatever(tc, vec2(0.1, 0.4)*-1)*0.8;

    //  colorA = texture2D(tex0,uv2.xy).xyz;
    //  colorB = texture2D(tex1,uv2.xy).xyz;
    // final += (colorA+colorB)*0.1;

    // final = pow(final, vec3(1.0/1.4));
    // linToSrgb(final);

    out_Color = vec4(final, 1.0);
}