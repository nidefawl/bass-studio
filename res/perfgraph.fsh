uniform sampler2D tex0;
uniform vec4 u_renderInfo;
uniform vec4 u_renderColor;
uniform vec2 u_viewport;
uniform float u_time;
in vec2 pass_texcoord;
in vec4 pass_position;
out vec4 outColor;

const vec2 INV_TEXSIZE = vec2(1.0/TEXTURE_WIDTH, 1.0/TEXTURE_HEIGHT);


float aastep(float threshold, float value) {
  #ifdef GL_OES_standard_derivatives
    float afwidth = length(vec2(dFdx(value), dFdy(value))) * 0.70710678118654757;
    return smoothstep(threshold-afwidth, threshold+afwidth, value);
  #else
    return step(threshold, value);
  #endif  
}


#define CHANNEL u_renderInfo.y
#define WIDTH_GRAPH u_renderInfo.z
#define HEIGHT_GRAPH u_renderInfo.w
float getGraphSample(vec2 tc) {
    // float pixX = (tc.x * WIDTH_GRAPH) + (TEXTURE_WIDTH-WIDTH_GRAPH);
    float pixX = (1.0 - tc.x) * WIDTH_GRAPH;
    ivec2 texelPos = ivec2(TEXTURE_WIDTH - 1 - floor(pixX), int(CHANNEL));
    return texelFetch(tex0, texelPos, 0).r;
}
void main(void) {
        float sampleVal = getGraphSample(pass_texcoord);
        // sampleVal = float(sampleVal > 0.0);
        // float sampleValB = getGraphSample(pass_texcoord + vec2(1.0/TEXTURE_WIDTH, 0.0));
        float sampleHeight = max(0.0, sampleVal - pass_texcoord.y);
        sampleHeight = smoothstep(0.0, 2.0/HEIGHT_GRAPH, sampleHeight);
        sampleHeight = aastep(0.0f, sampleHeight);
        float colorIntens = 0.7 + 0.8*max(0, pow(max(0.0, sampleVal - 0.75), .25));

        vec4 result = vec4(u_renderColor.rgb * colorIntens * sampleHeight, 1.0);
        // outColor = vec4(vec3(1), float(result.r!=0))
        outColor = vec4(result.rgb + u_renderColor.bgr*(1.0-pass_texcoord.y)*0.2, 1.0);
}