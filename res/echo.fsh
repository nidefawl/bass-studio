#version 150 core

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec4 params;
uniform float u_time;
uniform float u_clock;

in vec2 pass_texcoord;

out vec4 out_Color;

vec4 sampleShifted(sampler2D tex, vec2 tc) {
    vec2 texDim = textureSize(tex0, 0);
    float step = 1.0f/texDim.x;
    vec2 offset = vec2(step*1, 0);
    vec2 offset1 = vec2(0, 0);
    vec2 offset2 = vec2(step*-1, 0);
    vec3 s1 = texture(tex, tc+offset, 0).rgb;
    vec4 s2 = texture(tex, tc+offset1, 0);
    vec3 s3 = texture(tex, tc+offset2, 0).rgb;
    return vec4(s1.r, s2.g, s3.b, s2.a);
}
vec2 doWhatever(vec2 mouse, vec2 tc) {
    vec2 p = -1.0 + 2.0 * tc;
    vec2 m = -1.0 + 2.0 * mouse;

    float a1 = atan(p.y-m.y,p.x-m.x);
    float r1 = sqrt(dot(p-m,p-m));
    float a2 = atan(p.y+m.y,p.x+m.x);
    float r2 = sqrt(dot(p+m,p+m));
    vec2 uv;
    uv.x = 0.01*u_clock + (r1-r2)*0.25;
    uv.y = sin(1.0*(a1-a2))*0.5+0.5;

    float w = r1*r2*0.1;
    uv = mod(uv, vec2(1.0));
    uv = vec2(abs(uv.x-0.5)*2, abs(uv.y-0.5)*2);
    return uv;
}
void main(void) {

     vec4 colInput = texture(tex0, pass_texcoord.st, 0);
     vec4 color = colInput; 
     if (params.x < params.y-1) {
        // vec4 feedback = sampleShifted(tex1, pass_texcoord.st);
        // vec2 tc2 = doWhatever(vec2(params.x/(params.y-1)*0.5), pass_texcoord.st);
        vec4 feedback = texture(tex1, pass_texcoord.st);
        // feedback = colInput*feedback*0.2+feedback;
        // feedback.b *=1.2;
        // if (feedback.r > 1) feedback.r = 1.0-feedback.r;
        // if (feedback.g > 1) feedback.g = 1.0-feedback.g;
        // if (feedback.b > 1) feedback.b = 1.0-feedback.b;
        // float f = clamp(sin(u_clock*1.5+5), 0.0, 1);
        // float f2 = sin(u_time*1.3)*sin(u_time*2.3)*0.3+1.2;
        // float f3 = clamp(sin(u_time*2.5)+1.0, 0.0, 2.0)/2.0;
        // float f4 = 1.0-pow(abs((pass_texcoord.y-0.5))/8.5, 4.0);
        // color = vec4(1);// mix(colInput*7, feedback, 0.61)*222.9999;
        // f4 = pow(f4, 4.0); 
        color.rgb *= 0.66;
        color.rgb += feedback.rgb*0.33;
        color.rgb = tanh(vec3(color.rgb));

     }
     // color.a *= 0.99;
        // color *= 1;
     color.rgb = vec3(0.2);
     // co

     // color.rgb = ;
     // if (color > 1.)
     // color*=color;
     out_Color = vec4(color.rgb, color.a);
     // if (color.r > 1 || color.g > 1 || color.b > 1)
     //    out_Color = vec4(1,0,0,1);
}