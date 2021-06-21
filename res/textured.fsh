#version 150 core

uniform sampler2D tex0;

// in vec4 pass_Color;
in vec2 pass_texcoord;
 
out vec4 out_Color;
float isInside(vec2 pos, vec2 bottomLeft, vec2 topRight) {
    vec2 v = step(bottomLeft, pos)-step(topRight, pos);
    return v.x*v.y;
}
void main(void) {
	vec4 tex = texture(tex0, pass_texcoord.st, 0);
    // if (tex.a < 0.04)
    // 	discard;
    // out_Color = tex*pass_Color;




    //#define BORDER 0.005
	#ifdef BORDER
    out_Color = tex+vec4(1)*(1.0-isInside(pass_texcoord.st, vec2(BORDER), vec2(1.0-BORDER)));
	#else
	out_Color = tex;
	#endif
}