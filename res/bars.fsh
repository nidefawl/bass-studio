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
	if (pass_attr.x < 1) {
		discard;
   		return pass_color;
	} else {
		float a;
		if (pass_texcoord.y < 0.5) {
			vec2 scaled = pass_texcoord.xy;
			a = length(scaled - vec2(0.5));
		} else {
			a = abs(pass_texcoord.x-0.5);
		}
		a *= 1.12;
		a = 1.0 - a;
		a -= 0.5;
		a *= 8;
		a = clamp(a, 0.0, 1.0);
		float inverseTexcoordY = pass_attr.x-1.0;
		vec3 vc = vec3(0.0, 0.33, 0.66);
		float band = pass_attr.y/u_numbands;
		float lvl = pass_attr.z;
		lvl = 1.0-lvl;
		lvl = pow(lvl, 8.0);
		lvl = 1.0-lvl;
		float param = 0.0;
		param = pass_texcoord.y/16.0;
		param = (pass_texcoord.y/4.0*lvl);
		vec3 col = palette( lvl*1.4+0.2, vec3(0.5),vec3(0.5),vec3(0.5), vec3(0.5));
		vec3 color = col;
		return vec4(color, a);
	}
}

vec4 steppedBars(void) {
	if (pass_attr.x < 1) {
		discard;
   		return pass_color;
	} else {
		float a;
		a = abs(pass_texcoord.x-0.5);
		a *= 1.05;
		a = 1.0 - a;
		a -= 0.5;
		a *= 32;
		a = clamp(a, 0.0, 1.0);
		vec2 pos = pass_attr2.zw;
		vec2 relpos = pos / barSize;
		float fboxsize = barSize.x;
		float fstep = mod(pos.y, fboxsize);
		float fborder = 1;
		fstep = step(fborder, fstep)-step(fboxsize-fborder, fstep);
		if (pos.y > int(barSize.y/fboxsize)*fboxsize){
			a = 0;
		}
		float band = pass_attr.y/u_numbands;
		float lvl = pass_attr.z;
		lvl = 1.0-lvl;
		lvl = pow(lvl, 8.0);
		lvl = 1.0-lvl;
		float param = (pass_texcoord.y/4.0*fstep);
		vec3 col = palette( lvl+sin(u_time*0.1), vec3(0.5),vec3(0.5),vec3(2.0, 1.0, 0.1), vec3(0.23,0.22,0.2));
		vec3 color = col;
		return vec4(color, a*fstep);
	}
}
vec4 steppedBars2(void) {
	if (pass_attr.x < 1) {
		discard;
   		return pass_color;
	} else {
		float a;
		a = abs(pass_texcoord.x-0.5);
		a *= 1.05;
		a = 1.0 - a;
		a -= 0.5;
		a *= 32;
		a = clamp(a, 0.0, 1.0);
		vec2 pos = pass_attr2.zw;
		vec2 relpos = pos / barSize;
		float fboxsize = barSize.x;
		float fstep = mod(pos.y, fboxsize);
		int nBoxes = int(floor(barSize.y/fboxsize));
		float fquantizedStep = float(int(pos.y/fboxsize));
		float fquantizedStepRev = nBoxes-fquantizedStep;
		float fborder = min(2.0, fboxsize*(1.0/8.0));
		// float box = floor((barSize.y-pos.y) / fboxsize);

		float band = pass_attr.y/u_numbands;
		float lvl = pass_attr.z;
		lvl = 1.0-clamp(lvl, 0.0, 1.0);

		// lvl = pow(lvl, 2.0+(sin(u_time*1.9)+1.0)*4.0);
		lvl = pow(lvl, 4.0);
		lvl = 1.0-clamp(lvl, 0.0, 1.0);
		float lvlPeak = 0.8-fquantizedStepRev/10.0;
		float lvl2 = clamp(((lvl-lvlPeak)/(1.0-lvlPeak))*fquantizedStepRev, 0.0, 1.0);
		float param = clamp(lvl*0.1+pass_attr.w, 0.0, 1.0);
		float progress = clamp(sin(u_time*0.4)-0.5, 0.0, 0.5)/0.5;
		float progress2 = clamp(sin(u_time*0.2)-0.8, 0.0, 0.2)/0.2;
		vec3 vc = mix(vec3(0.33,0.66,0.0), vec3(0.0,0.33,0.66), 0);
		vec3 vc3 = mix(vec3(1.2, 1.0, 0.1), vec3(0.6, 0.9, 0.4), progress2);
		// vec3 vc = vec3(0.0,0.33,0.66);
		// vec3 vc3 = vec3(1);
		vec3 col = palette( param, vec3(0.28),vec3(0.4),vc3, vc);
		vec3 color = col;//vec3(0.3, 0.3, 0.9);
		if (pos.y > int(barSize.y/fboxsize)*fboxsize){
			a = 0;
		} else {
			float fstep2 = clamp((fstep-fborder)/(fboxsize-fborder*2.0), 0.0, 1.0);
			vec2 scaled = vec2(clamp((pass_texcoord.x-0.05)/0.9, 0.0, 1.0), fstep2);
			float rndScale = 1.0-length(scaled - vec2(0.5+sin(u_time+band+fquantizedStep)*0.05, 0.5+cos(u_time+band+fquantizedStep)*0.05));
			float rndInset = 0.2;
			float rndBorder = clamp((((rndScale-rndInset)/(1.0-2.0*rndInset))-0.5)*1.0, 0.0, 1.0);
			// a*=rndBorder;
			color *= lvl*0.3+0.3+0.6*vec3(rndBorder);
			// color *= 1.4-lvl2*0.5;
			color *= 1.0+lvl2*0.8;
			color.r *= 1.0+(rndScale)*2.0;
			color.g *= 1.0+(rndScale)*1.6;
			color *= 1.0+(pass_attr.w);
			// a = ;
			fstep = step(fborder, fstep)-step(fboxsize-fborder, fstep);
			// color.r *= 1.0+(1.0-length(scaled - vec2(0.5)))*1.;
		}

		return vec4(pow(clamp(color, vec3(0), vec3(1)), vec3(0.5+fstep*1)), a*fstep);
	}
}
vec4 steppedBars3(void) {
	float boxScale = 1;
	float boxSizeY = max(4.0, barSize.x*boxScale);
    float tcHeight = barSize.y/barSize.x;
    float ph = mod(tcHeight - pass_texcoord.y, boxScale)*(1.0/boxScale);
    float ph2 = mod(pass_texcoord.x, 1.0);
    float inBoxY = clamp((1.0-abs(ph-0.5)*2)*12.0-0.7, 0, 1);
    float inBoxX = clamp((1.0-abs(ph2-0.5)*2)*12.0-0.7, 0, 1);
    inBoxY = smoothstep(0, 1, inBoxY);
    inBoxX = smoothstep(0, 1, inBoxX);
	vec2 pos = pass_attr2.zw;
	float nBoxes = floor(barSize.y/boxSizeY);
    float inFullBoxY = clamp(nBoxes*boxSizeY - pos.y - 2, 0.0, 1.0);

	float fquantizedStep = floor(pos.y/boxSizeY);
	float fquantizedStepRev = nBoxes-fquantizedStep;

	vec2 relpos = pos / barSize;
	float fstep = clamp(ph*1.1-0.03, 0, 1)*boxSizeY;
	float fborder = min(2.0, boxSizeY*(1.0/8.0));


	float band = pass_attr.y/u_numbands;
	float lvl = pass_attr.z;
	// lvl = 1.0-clamp(lvl, 0.0, 1.0);

	// lvl = pow(lvl, 4.0);
	// lvl = 1.0-clamp(lvl, 0.0, 1.0);
	// float lvlPeak = 0.8-fquantizedStepRev/10.0;
	// float lvl2 = clamp(((lvl-lvlPeak)/(1.0-lvlPeak))*fquantizedStepRev, 0.0, 1.0);
	float param = clamp(lvl*2.0, 0.0, 1.0);
	// float progress = clamp(sin(u_time*0.4)-0.5, 0.0, 0.5)/0.5;
	float progress2 = clamp(sin(u_time*0.2)-0.8, 0.0, 0.2)/0.2;
	vec3 vc3 = mix(vec3(0.5, 0.5, 0.4), vec3(0.3, 0.3, 0.3), 0);

    float phase = clamp(((sin((-u_time+band*2.0+fquantizedStep)*4.0))), 0.0, 1.0);

	float fstep2 = clamp((fstep-fborder)/(boxSizeY-fborder*2.0), 0.0, 1.0);
	vec2 scaled = vec2(clamp((pass_texcoord.x-0.05)/0.9, 0.0, 1.0), fstep2);
	float rndScale = 1.0-length(scaled - vec2(0.5, 0.5));
	float rndInset = 0.1;
	float rndBorder = clamp((((rndScale-rndInset)/(1.0-2.0*rndInset))-0.5)*6.0, 0.0, 1.0);
	vec3 col = palette( band*(fquantizedStepRev*0.1), 
		vec3(0.5),
		mod(vec3(lvl)*1.5, vec3(1.0))*0.5+0.2,
		vc3, 
		mod(vec3(0.7,0.33,0.16)*0.9, vec3(1.0)));
	rndBorder*=rndBorder;
	vec3 color = col.rgb;
	// rndBorder*=rndBorder;

	// color *= lvl*0.3+0.3+0.6*vec3(rndBorder);
	color *= vec3(rndBorder);
	// color *= 1.4-lvl2*0.5;
	// color *= 0.5+lvl2*0.5;
	color.r *= 1.0+(rndScale)*2.0;
	color.g *= 1.0+(rndScale)*1.6;
	color *= 1.0+(pass_attr.w);
	
	fstep = step(fborder, fstep)-step(boxSizeY-fborder, fstep);

    // color.rgb = clamp(color.rgb, vec3(0), vec3(1.2));
    // color = mix(color, pow(color, vec3(4.0)), mix(0.0, phase, progress));
    // color.rgb = clamp(color.rgb, vec3(0), vec3(1.2));
	return vec4(color, inBoxY*inBoxX*inFullBoxY);
}

vec4 steppedBars4(void) {
	float boxScale = 1;
	float boxSizeY = max(4.0, barSize.x*boxScale);
    float tcHeight = barSize.y/barSize.x;
    float ph = mod(tcHeight - pass_texcoord.y, boxScale)*(1.0/boxScale);
    float ph2 = mod(pass_texcoord.x, 1.0);
    float inBoxY = clamp((1.0-abs(ph-0.5)*2)*12.0-0.7, 0, 1);
    float inBoxX = clamp((1.0-abs(ph2-0.5)*2)*12.0-0.7, 0, 1);
    inBoxY = smoothstep(0, 1, inBoxY);
    inBoxX = smoothstep(0, 1, inBoxX);
	vec2 pos = pass_attr2.zw;
	float nBoxes = floor(barSize.y/boxSizeY);
    float inFullBoxY = clamp(nBoxes*boxSizeY - pos.y - 2, 0.0, 1.0);

	float fquantizedStep = floor(pos.y/boxSizeY);
	float fquantizedStepRev = nBoxes-fquantizedStep;

	vec2 relpos = pos / barSize;
	float fstep = clamp(ph*1.1-0.03, 0, 1)*boxSizeY;
	float fborder = min(2.0, boxSizeY*(1.0/8.0));

	float ftime = u_time*0.7;

	float band = pass_attr.y/u_numbands;
	float lvl = pass_attr.z;
	lvl = 1.0-clamp(lvl, 0.0, 1.0);

	// lvl = pow(lvl, 2.0+(sin(ftime*1.9)+1.0)*4.0);
	lvl = pow(lvl, 4.0);
	lvl = 1.0-clamp(lvl, 0.0, 1.0);
	float lvlPeak = 0.8-fquantizedStepRev/64.0;
	float lvl2 = clamp(((lvl-lvlPeak)/(1.0-lvlPeak))*fquantizedStepRev, 0.0, 1.0);
	float param = clamp(lvl*0.15+(fquantizedStepRev/barSize.y)*pass_attr.w*24, 0.0, 1.0);
	float progress = clamp(sin(ftime*0.4)-0.5, 0.0, 0.5)/0.5;
	float progress2 = clamp(sin(ftime*0.2)-0.8, 0.0, 0.2)/0.2;
	float progress3 = clamp(sin(ftime*2.2+4202)-0.5, -0.5, 0.5)/0.5;
	vec3 col2 = palette( param, vec3(0.3),vec3(0.5),vec3(1.2, 1.0, 0.1), vec3(0.33,0.33,0.66-0.1));
	col2 = clamp(col2, vec3(0), vec3(1));
	// vec3 vc = vec3(0.0,0.33,0.66);
	// vec3 vc3 = vec3(1);
    float phase = clamp(((sin((-ftime+band*2.0+fquantizedStep)*4.0))), 0.0, 1.0);
    float phase2 = clamp(((sin((ftime*12.0/5.0+band*1.5+fquantizedStep*0.3)*4.0+441)-1.5)*2.0), 0.0, 1.0);
    phase *= progress;
    phase2 *= progress;
	vec3 vc = mix(vec3(0.18, 0.25+sin(ftime)*0.2, 0.3+sin(ftime*1.33*0.3)*0.3), vec3(0.0,0.33,0.66), 0.0);
	vec3 vc3 = mix(vec3(0.5+sin(ftime*0.66+1000)*0.1), vec3(0.6, 0.9, 0.4), 0);
	vec3 col = palette( pass_attr.w*pass_attr.w, vec3(0.5),vec3(mix(max(0.5-pass_attr.w*0.2, 0.00), min(1.0, phase+phase2), progress)),vc3, vc);
	vec3 color = col;//pow(col, vec3(4))*0.3+0.1;//vec3(0.3, 0.3, 0.9);

	float fstep2 = clamp((fstep-fborder)/(boxSizeY-fborder*2.0), 0.0, 1.0);
	vec2 scaled = vec2(clamp((pass_texcoord.x-0.05)/0.9, 0.0, 1.0), fstep2);
	float rndScale = 1.0-length(scaled - vec2(0.5+sin(ftime+band+fquantizedStep)*0.05, 0.5+cos(ftime+band+fquantizedStep)*0.05));
	float rndInset = 0.2;
	float rndBorder = clamp((((rndScale-rndInset)/(1.0-2.0*rndInset))-0.5)*1.0, 0.0, 1.0);
	color*=0.9;
	color = clamp(color, vec3(0), vec3(1));
	color *= lvl*0.3+0.3+0.6*vec3(rndBorder);
	color *= 1.0+lvl2*2.8;

	color+=col2*0.09;
	color.r *= 0.9+(rndScale)*1.9;
	color.g *= 0.95+(rndScale)*1.5;
	color *= 1.0+(pass_attr.w);
	// color = vec3(rndScale);

	// fstep = step(fborder, fstep)-step(boxSizeY-fborder, fstep);
	fstep = smoothstep(0, fborder, fstep)-smoothstep(boxSizeY-fborder, boxSizeY, fstep);


    color = mix(color, pow(color, vec3(1.8)), mix(0, min(1.0, phase+phase2), progress));

	return vec4(color, inBoxY*inBoxX*inFullBoxY);
}
void main(void) {
	#if PRESET == 0
		vec4 color = steppedBars3();
	#elif PRESET == 1
		vec4 color = steppedBars4();
	#elif PRESET == 2
		vec4 color = roundBars();
	#else
		vec4 color = rectBars();
	#endif
	out_Color = color;
}