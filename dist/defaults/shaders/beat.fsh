
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
    float t = iTime;
    float srd = sawRampDown(t * 1.0);
    srd = pow(srd, 4.0);
    float pul = 1.0;//pulse(t * 1.0/8.0, 0.125);
    float pluck = max(0.0, pul * srd);
    vec3 col = mix(vec3(0), vec3(0.1, 0.2, 1.0), pluck);
    fragColor = vec4(col,1.0);
}
