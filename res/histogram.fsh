#version 150 core


#define TEXTURE_IS_LINSCALE 1

uniform sampler2D tex0;
uniform sampler2D tex1;

uniform vec4 params;
uniform float u_time;
uniform float u_clock;
uniform float u_since;
uniform vec2 u_viewport;

in vec2 pass_texcoord;

out vec4 out_Color;

//make this a uniform
#define SAMPLERATE 44100.0
#define SR_OVER_FFT (44100.0/(512.0*4.0)) 
#define FFT (512.0*4.0) 
float log10(float x) {
    return log(x)/log(10.0);
}
float dBFS(float f) {
    return 20.0 * log10(f);
}
const float MIN_FREQ = 20.0;
const float MAX_FREQ = 22000.0;
const float DBFS_FLOOR = -120.0f;
const float MTR_FLOOR = -90.0f;
const float MTR_CEIL = 0.0f;
// const float minLog10 = 1.30102999566398119521373889472449302676818988146211; //20 hz
// const float maxLog10 = 4.34242268082220623596393886596751726847489207192856; //22000 hz
const float minLog10 = log(MIN_FREQ)/log(10.0); //20 hz
const float maxLog10 = log(MAX_FREQ)/log(10.0); //22000 hz
const float range = maxLog10 - minLog10;
float scaledRange(float db, float lvlFloor, float lvlCeil) {
    if (db < DBFS_FLOOR)
        return 1.0f;
    float lvlRange = lvlFloor - lvlCeil;
    return (max(lvlFloor, min(db, lvlCeil)) - lvlCeil) / lvlRange;
}
vec3 palette( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b*cos( 6.28318*(c*t+d) );
}
#if TEXTURE_IS_LINSCALE == 1
float getFFTLevelDB(float f) {
    float freq = pow(10, range*f+minLog10);
    float bin = freq / SAMPLERATE; //get rid of division
    float binQuantized = floor(freq / SR_OVER_FFT)/FFT;
    // bin = mix(bin, binQuantized, step(sin(u_time*2.5), 0.5));
    float fft = texture(tex0, vec2(bin, 0), 0).r;
    float db = dBFS(fft);
    float scale = 1.0-scaledRange(db, MTR_FLOOR, MTR_CEIL);
    return scale;
}
#else
float getFFTLevelDB(float f) {
    float fft = texture(tex0, vec2(f, 0), 0).r;
    float db = dBFS(fft);
    float scale = 1.0-scaledRange(db, MTR_FLOOR, MTR_CEIL);
    return scale;
}
#endif

vec4 histogram(float pos, float tc, float width, vec2 dir, int stepLen) {

     vec4 col = vec4(1);
     if (pos >= width-stepLen) {
        vec2 t = pass_texcoord*dir;
        float scale = getFFTLevelDB(tc);
        // scale = sin(u_clock*20);
        col.rgb = palette( scale, vec3(0.5),vec3(1),vec3(1),vec3(0.33,0.66,0.99)*0.4+0.05)*(scale*1);
     } else {
        ivec2 texelOffset = ivec2(gl_FragCoord.xy + dir * stepLen);
        col.rgb = texelFetch(tex1, texelOffset, 0).rgb;
     }
     return col;
}

vec4 histogramX(int stepLen) {
     return histogram(gl_FragCoord.x, pass_texcoord.y, u_viewport.x, vec2(1,0), stepLen);
}
vec4 histogramY(int stepLen) {
     return histogram(gl_FragCoord.y, pass_texcoord.x, u_viewport.y, vec2(0, 1), stepLen);
}

void main(void) {
    int stepLen = 16;
     vec4 col = histogramX(stepLen);
     out_Color = col;   
}