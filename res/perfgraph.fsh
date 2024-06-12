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
    float afwidth = length(vec2(dFdx(value), dFdy(value))) * 0.70710678118654757;
    return smoothstep(threshold-afwidth, threshold+afwidth, value);
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
        float peakHeight = 1.5 + sampleVal * 1.0;
        float minValToSeeLineAtBottomOfScreen = 3.0/HEIGHT_GRAPH;
        sampleVal = minValToSeeLineAtBottomOfScreen + sampleVal * (1.0 - minValToSeeLineAtBottomOfScreen);
        float sampleOnPos = max(0.0, sampleVal - pass_texcoord.y + peakHeight/HEIGHT_GRAPH * 0.5);
        float sampleOffPos = max(0.0, sampleVal - pass_texcoord.y - peakHeight/HEIGHT_GRAPH * 0.5);
        float s = smoothstep(0.0, 2.0/HEIGHT_GRAPH, sampleOnPos);
        float s2 = smoothstep(2.0/HEIGHT_GRAPH, 0.0, sampleOffPos);
        float yAtPeak = s * s2;
        vec3 peakColor = vec3(1.0);
        vec3 areaColor = u_renderColor.rgb;
        vec3 col = mix(areaColor, peakColor, yAtPeak);
        float alpha = s;
        outColor = vec4(col, alpha);
}