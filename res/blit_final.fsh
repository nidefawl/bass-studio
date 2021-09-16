#version 150 core

uniform sampler2D tex0;
uniform float u_time;

in vec2 pass_texcoord;
out vec4 out_Color;

vec3 sampleShifted(sampler2D tex, vec2 tc, vec2 offset, vec2 offset1, vec2 offset2) {
	vec3 s1 = texture(tex, tc+offset, 0).rgb;
	vec3 s2 = texture(tex, tc+offset1, 0).rgb;
	vec3 s3 = texture(tex, tc+offset2, 0).rgb;
	return vec3(s1.r, s2.g, s3.b);
}
vec3 sampleNeigbours(sampler2D tex, vec2 tc, int stepR) {
	vec2 texDim = textureSize(tex0, 0);
	vec2 texStp = 1.0 / texDim;
	vec3 acc = vec3(0);
	const int nR=4;
	for (int x = -nR; x < nR+1; x++) {
		for (int y = -nR; y < nR+1; y++) {
			vec3 neiSample = texture(tex, tc + texStp * vec2(x*stepR, y*stepR), 0).rgb;
			acc += neiSample;
		}	
	}
	return acc;
}
float triFade(float tmSec, float lenSec, float freqSec) {
	/* ____/\____  lenSec anim (freqSec-lenSec)/2 sleep (freqSec cycle) */
	float tmOffset = lenSec + (freqSec - lenSec) * 0.5;
	float tmProgr = mod(tmSec + tmOffset, freqSec);
	float fadeTri = clamp(1.0 - abs(tmProgr*2.0/lenSec - 1.0), 0.0, 1.0);
	return smoothstep(0.0, 1.0, fadeTri);
}
float vignette(vec2 texCoord) {
    texCoord *=  1.0 - texCoord.yx;
    float vig = texCoord.x*texCoord.y * 15.0; // multiply with sth for intensity
    return pow(vig, 0.25); // change pow for modifying the extend of the  vignette
}
vec3 palette( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b*cos( 6.28318*(c*t+d) );
}
vec3 paletteIdx(in float t, in float palIdx) {
    vec3             col = palette( t, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,1.0,1.0),vec3(0.0,0.33,0.67) );
    if( palIdx >(0.5) ) col = palette( t, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,1.0,1.0),vec3(0.0,0.10,0.20) );
    if( palIdx >(1.5) ) col = palette( t, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,1.0,1.0),vec3(0.3,0.20,0.20) );
    if( palIdx >(2.5) ) col = palette( t, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,1.0,0.5),vec3(0.8,0.90,0.30) );
    if( palIdx >(3.5) ) col = palette( t, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,0.7,0.4),vec3(0.0,0.15,0.20) );
    if( palIdx >(4.5) ) col = palette( t, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(2.0,1.0,0.0),vec3(0.5,0.20,0.25) );
    if( palIdx >(5.5) ) col = palette( t, vec3(0.8,0.5,0.4),vec3(0.2,0.4,0.2),vec3(2.0,1.0,1.0),vec3(0.0,0.25,0.25) );
	return col;
}
float bpm2Tm(float bpm) {
	return 60000.0/bpm;
}
float luminance(vec3 rgb)
{
    return dot(rgb, vec3(0.2125, 0.7154, 0.0721));
}
//note: uniformly distributed, normalized rand, [0;1[
float nrand( vec2 n )
{
	return fract(sin(dot(n.xy, vec2(12.9898, 78.233)))* 43758.5453);
}
float n3rand( vec2 n, float rt )
{
	float t = fract( rt );
	float nrnd0 = nrand( n + 0.07*t );
	float nrnd1 = nrand( n + 0.11*t );
	float nrnd2 = nrand( n + 0.13*t );
	return (nrnd0+nrnd1+nrnd2) / 3.0;
}
const float u_bpm = 128.0;
vec3 shade1(float fTime, vec2 tc) {
	vec3 c0 = texture(tex0, tc).rgb*0.01;
	float fTm1 = triFade(fTime, bpm2Tm(u_bpm)*16.0, bpm2Tm(u_bpm)*16.0);
	float fTm2 = triFade(fTime, bpm2Tm(u_bpm)*32.0, bpm2Tm(u_bpm)*32.0);
	vec3 c1 = pow(c0, vec3(2.7));
	float vign = vignette(tc.xy);
	float vignStrong = pow(vign, 8.0);
	vec2 texDim = textureSize(tex0, 0);
	float texStp = ((vign)*fTm1*4.0)/texDim.x;
	vec3 sampleA = sampleShifted(tex0, tc.st, vec2(texStp*1, 0), vec2(00), vec2(texStp*-1, 0));
	vec3 colFinal = mix(c1, sampleA, vignStrong);
    vec3 paletteColor2 = pow(paletteIdx( vign, 5. ), vec3(1.7));
	return pow((colFinal-paletteColor2*0.1-vign*0.1), vec3(1.0/2.2));
}
vec3 shade2(float fTime, vec2 tc) {
	float normalizedNoise = n3rand( tc.xy, fTime );
	float normalizedNoise2 = n3rand( (vec2(1.0)-tc.xy)*1.4+vec2(0.3) , fTime );
	float antiBandingDither = (-0.5+2.0*normalizedNoise)/256.0; // for 8 bit output

	vec3 c0 = texture(tex0, tc).rgb*0.1;
	float fTm1 = triFade(fTime, bpm2Tm(u_bpm)*16.0, bpm2Tm(u_bpm)*16.0);
	float fTm2 = triFade(fTime, bpm2Tm(u_bpm)*32.0, bpm2Tm(u_bpm)*32.0);
	vec3 c1 = pow(c0, vec3(0.7));
	float vign = vignette(tc.xy);
	float vignStrong = pow(vign, 8.0);
	vec2 texDim = textureSize(tex0, 0);
	float texStp = ((vign)*fTm1*23.0)/texDim.x;
	vec3 sampleA = sampleShifted(tex0, tc.st, vec2(texStp*1, 0), vec2(0.), vec2(texStp*-1, 0))*0.2;
	// vec3 colFinal = mix(c1, sampleA, vignStrong);
	// float neigLum = luminance(sampleNeigbours(tex0, tc, 4)*(1.0/600.0));

	vec3 colFinal = pow(
		paletteIdx( (0.2+(fTm2*vign)*0.13)*20.8+ normalizedNoise2*0.05 , 5. )*0.5	
		+ sampleA, 
		vec3(2.7));

	// mod((tc.x+fTm1), 1.0)*5.0
	return pow(colFinal, vec3(1.0/2.2)) + vec3(antiBandingDither);
}
vec3 shadeNone(float fTime, vec2 tc) {
	vec3 c0 = texture(tex0, tc).rgb;
	return c0;
}

void main(void) {
	float fTm1 = triFade(u_time+bpm2Tm(u_bpm)*8.0, bpm2Tm(u_bpm)*16.0, bpm2Tm(u_bpm)*16.0);
	out_Color = vec4(shade1(u_time, pass_texcoord), 1.0);
}