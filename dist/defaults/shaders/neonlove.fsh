
// Original:
// NEON LOVE by alro - https://www.shadertoy.com/view/WdK3Dz

/* 
 * Fixed the creases in the heart caused by the property of the distance field on concave areas.
 * By converting the individual bezier SDF segments into light/glow first,
 * we can treat them as indiviual lights and sum them together.
 * This does make the endpoints to double in intensity as they overlap,
 * so we subtract light on the endpoints to get the smooth lighting properly.
 *
 */

#define PI 3.14159265359
#define TWO_PI 2. * PI
#define POINT_COUNT 32

vec2 points[POINT_COUNT];

const float eps = 1e-8;
float intensity = 1.25;
const float INIT_HEARTSCALE = 0.025;
float heartscale = INIT_HEARTSCALE;
float radius = INIT_HEARTSCALE * 0.15;
float thickness = INIT_HEARTSCALE * 0.02;
int quantize = 0;

//https://www.shadertoy.com/view/MlKcDD
//Signed distance to a quadratic bezier
float sdBezier(vec2 pos, vec2 A, vec2 B, vec2 C){    
    vec2 a = B - A;
    vec2 b = A - 2.0*B + C;
    vec2 c = a * 2.0;
    vec2 d = A - pos;

    float kk = 1.0 / dot(b,b);
    float kx = kk * dot(a,b);
    float ky = kk * (2.0*dot(a,a)+dot(d,b)) / 3.0;
    float kz = kk * dot(d,a);      

    float res = 0.0;

    float p = ky - kx*kx;
    float p3 = p*p*p;
    float q = kx*(2.0*kx*kx - 3.0*ky) + kz;
    float h = q*q + 4.0*p3;
    
    if(h >= 0.0){ 
        h = sqrt(h);
        vec2 x = (vec2(h, -h) - q) / 2.0;
        vec2 uv = sign(x)*pow(abs(x), vec2(1.0/3.0));
        float t = uv.x + uv.y - kx;
        t = clamp( t, 0.0, 1.0 );

        // 1 root
        vec2 qos = d + (c + b*t)*t;
        res = length(qos);
    }else{
        float z = sqrt(-p);
        float v = acos( q/(p*z*2.0) ) / 3.0;
        float m = cos(v);
        float n = sin(v)*1.732050808;
        vec3 t = vec3(m + m, -n - m, n - m) * z - kx;
        t = clamp( t, 0.0, 1.0 );

        // 3 roots
        vec2 qos = d + (c + b*t.x)*t.x;
        float dis = dot(qos,qos);
        
        res = dis;

        qos = d + (c + b*t.y)*t.y;
        dis = dot(qos,qos);
        res = min(res,dis);

        qos = d + (c + b*t.z)*t.z;
        dis = dot(qos,qos);
        res = min(res,dis);

        res = sqrt( res );
    }
    
    return res;
}

//http://mathworld.wolfram.com/HeartCurve.html
vec2 getHeartPosition(float x){
    x *= TWO_PI;
    x*=0.5;
    return vec2(16.0 * sin(x) * sin(x) * sin(x),
                (13.0 * cos(x) - 5.0 * cos(2.0*x)
                - 2.0 * cos(3.0*x) - cos(4.0*x))) * heartscale;
}

vec2 func(float x){
    return vec2(x, sin(x * iTime * TWO_PI));
}

//https://www.shadertoy.com/view/3s3GDn
float getGlow(float dist, float radius, float intensity){
    return pow(radius*dist, intensity);
}

float getLooped(float t, vec2 pos){
	for(int i = 0; i < POINT_COUNT; i++){
        float ff = float(i);
        float x = ff/float(POINT_COUNT-1);
        x = -1.0 + 2.0 * x;
        points[i] = getHeartPosition(x);
    }
	float light = 0.;
    for(int i = -1; i < POINT_COUNT-1; i++){
        //https://tinyurl.com/y2htbwkm
        int idx = i < 0 ? POINT_COUNT + i : i;
        int idxPrev = idx == 0 ? POINT_COUNT-1 : idx-1;
        int idxNext = idx == POINT_COUNT-1 ? 0 : idx+1;
        vec2 c_prev = (points[idxPrev] + points[idx]) / 2.0;
        vec2 c = (points[idx] + points[idxNext]) / 2.0;
        // Distance from bezier segment
        float d = sdBezier(pos, c_prev, points[idx], c);
        // Distance from endpoint (except from first point)
        float e = distance(pos, c_prev);
        // Convert the distance to light and accumulate
        light += ( 1. / max(d - thickness, eps));
        // Convert the endpoint as well and subtract
        light -= 1. / max(e - thickness, eps);
    }
    return max(0.0, light);
}
float getSegment(float t, vec2 pos, float offset, float len){
	for(int i = 0; i < POINT_COUNT; i++){
        float ff = float(i);
        float x = ff/float(POINT_COUNT-1);
        x *= len;
        x += offset;
        x = fract(x); 
        x = -1.0 + 2.0 * x;
        points[i] = getHeartPosition(x);
    }
    vec2 c = (points[0] + points[1]) / 2.0;
    vec2 c_prev;
	float light = 0.;
    
    for(int i = 0; i < POINT_COUNT-1; i++){
        //https://tinyurl.com/y2htbwkm
        c_prev = c;
        c = (points[i] + points[i+1]) / 2.0;
        // Distance from bezier segment
        float d = sdBezier(pos, c_prev, points[i], c);
        // Distance from endpoint (except from first point)
        float e = i > 0 ? distance(pos, c_prev) : 1000.;
        // Convert the distance to light and accumulate
        light += 1. / max(d - thickness, eps);
        // Convert the endpoint as well and subtract
        light -= 1. / max(e - thickness, eps);
    }
    return max(0.0, light);
}

vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d )
{
    return a + b*cos( 6.28318*(c*t+d) );
}
float tri(float x){
    return 1.0 - abs(1.0 - 2.0 * fract(x));
}
float sawRampUp(float x){
    return 1.0 * fract(x);
}
float sawRampDown(float x){
    return 1.0 - fract(x);
}
float pulse(float x, float pw){
    // pulse is a square wave with a duty cycle of pw
    float ph = mod(x, 1.0);
    return float(ph < pw);
}
void mainImage( out vec4 fragColor, in vec2 fragCoord ){
    vec2 uv = fragCoord/iResolution.xy;
    float widthHeightRatio = iResolution.x/iResolution.y;
    vec2 centre = vec2(0.5, 0.5);
    vec2 pos = uv - centre;
    pos.y /= widthHeightRatio;
    pos *= 2.0;
    //Shift upwards to centre heart
    pos.y -= 0.06;
	
    float t = iTime * 0.5;
    
    float phaseColor = sin(t * 0.66) * 0.5 + 0.5;
    vec3 pinkish = vec3(1.0, 0.05, 0.3);
    vec3 palCol = pal( phaseColor*1.0, vec3(0.5,0.5,0.5),vec3(0.5,0.5,0.5),vec3(1.0,1.0,1.0),vec3(0.0,0.33,0.67) );
    // linearize
    pinkish = pow(palCol, vec3(1./2.2));
    vec3 blueish = vec3(0.1, 0.2, 1.0);
    vec3 colA = mix(pinkish, blueish, phaseColor);
    vec3 colB = mix(blueish, pinkish, phaseColor);
    
    float srd = sawRampDown(t * 8.0);
    srd = pow(srd, 4.0);
    heartscale += pow(sawRampDown(t * 2.0), 4.0) * 0.02;
    float pul = pulse(t * 1.0/8.0, 0.125);
    float pluck = max(0.0, pul * srd);
    radius += pluck * 0.01;
    heartscale *=  1.0 + pluck * 0.02 * sawRampDown(t * 1.0);
    vec2 offset = vec2(sin(t*1.0*TWO_PI), cos(t*1.0*TWO_PI)) * 1.0;
    float sOffset, sLen;
    float tempoBit = 1.0/8.0;
    float tBit = mod(fract(t*tempoBit) * 2.0  + tempoBit * 2.0, 2.0);
    if (tBit < tempoBit) {
        float tq = fract((tBit / tempoBit) * 1.0);
        quantize = int((1.0 - pow(tq, 2.0)*0.5) * 10.0);
        float upscale = smoothstep(0.0, 1.0, 1.0-pow(1.0-tq, 4.0));
        heartscale *= 1.0 + upscale * 0.1;
        // radius += smoothstep(0.0, 1.0, pow(tq, 4.0)) * 0.05;
    } else if (tBit < tempoBit * 2.0) {
        float tq = (tBit - tempoBit) / (tempoBit);
        sLen = tq;
        tq = 1.0 - tq;
        sOffset = -(sLen*0.5);
        radius += smoothstep(0.0, 1.0, tq) * 0.1;
        heartscale *= 1.0 + smoothstep(0.0, 1.0, tq) * 0.1;
        intensity *= 1.0 - smoothstep(0.0, 1.0, tq) * 0.15;
    } else if (tBit < tempoBit * 3.0) {
        float tq = (tBit - tempoBit * 2.0) / (tempoBit);
        tq = 1.0 - tq;
        sLen = tq;
        sOffset = -(sLen*0.5);
        intensity *= 1.0 + smoothstep(0.0, 1.0, tq) * 0.5;
    } else {
        float tq = (tBit - tempoBit * 3.0) / (2.0 - tempoBit * 3.0);
        sLen = smoothstep(0.0, 1.0, tq);
        tq = tri(fract(tq));	
        tq = 1.0-pow(1.0-tq, 4.0);
        sLen *= 1.0 + tq*64.0;
        intensity *= 1.0 + smoothstep(0.0, 1.0, tq) * 0.15;
    }
    if (quantize != 0) {
        float fsteps = float(int(1) << quantize);
        vec2 posQuantized = (floor(pos * fsteps)+vec2(0.5)) / fsteps;
        pos = posQuantized;
    }
    
    int numLoops = pluck > 0.0 ? 3 : 1;
    vec3 col = vec3(0.0);
    for (int n = 0; n < numLoops; n++){
        float lightWhole = getLooped(t, pos);
        float lightSegment = getSegment(t, pos, 
                                        sOffset, 
                                        sLen
                                        );
        float glowWhole = getGlow(lightWhole, radius, intensity * 1.25);
        float glowSegment = getGlow(lightSegment, radius, intensity * 0.75);

        vec3 col1 = vec3(glowWhole);
        col1 += vec3(smoothstep(0., 1.0, glowWhole)) * max(0.0, sin(t * 2.0)) * 0.5;
        col1 *= colA;

        vec3 col2 = vec3(glowSegment);
        col2 += vec3(smoothstep(0., 1.0, glowSegment)) * max(0.0, sin(t * 2.0)) * 0.5;
        col2 *= colB;

        col += col1;
        if (n == 0)
            col += col2;
        pos += offset*heartscale * pluck;
    }

    //Tone mapping
    col = 1.0 - exp(-col);
    
    //Gamma
    col = pow(col, vec3(0.4545));

    //Output to screen
    fragColor = vec4(col,1.0);
}
