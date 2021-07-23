#version 150 core

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform float u_alpha;


in vec2 pass_texcoord;

out vec4 out_Color;

vec3 blurTex(sampler2D tex, vec2 tc) {
	vec3 c = vec3(0);
	int n = 64;
	float f = 1.0f/float(n);
	float extRang = max(0.0, (tc.y-0.55)/0.45);
	float rng = 0;
	rng+=(extRang*(1.0/4.0));
	// rng/=2.0;
	for (int i = 0; i < n; i++) {
		c += texture(tex, tc+vec2(0.0, (-1.0+2.0*i/float(n-1))*rng)).rgb*f;
		// c += vec4(0.1);
	}
	return c*3;
}
vec3 blurTex2(sampler2D tex, vec2 tc) {
	float extRang = max(0.0, (tc.y-0.5)/0.5);
	vec4 col = texture(tex, tc, 0);
	return col.rgb*0.6*extRang;
}
void main(void) {
#if PRESET == 0
	vec4 col = texture(tex0, pass_texcoord.st, 0);
	col.rgb += blurTex(tex0, pass_texcoord.st);
	col.rgb += blurTex2(tex1, pass_texcoord.st);
	col.a = 1.0;
#elif PRESET == 1
	vec4 col = texture(tex0, pass_texcoord.st, 0);
#endif
	col = clamp(col, vec4(0.0), vec4(1.0));
    out_Color = vec4(col.rgb, clamp(col.a, 0.0, 1.0));
}