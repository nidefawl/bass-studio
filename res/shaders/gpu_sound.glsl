#version 430
#define N_CHANNELS 2
#define N_SAMPLES 512
// N_SAMPLES * N_CHANNELS * float
layout(std430, binding = 0) buffer block_in
{
	float samples[N_SAMPLES * N_CHANNELS];
} audioblock_in;
layout(std430, binding = 1) buffer block_out
{
	float samples[N_SAMPLES * N_CHANNELS];
} audioblock_out;

layout(binding = 0) uniform context {
    float samplerate;
    float bpm;
    float time_seconds;
    float time_samples;
    float time_beats;
} ctx; 

layout(local_size_x = N_SAMPLES, local_size_y = 1, local_size_z = 1) in;

vec2 mainSound( in int s,float time);
void main()
{
    uint i = gl_LocalInvocationIndex.x;
    float sample_offset = ctx.time_samples + float(i);
    vec2 s_lr = mainSound(int(sample_offset), sample_offset / ctx.samplerate);
    audioblock_out.samples[i] = s_lr.x;
    audioblock_out.samples[i + N_SAMPLES] = s_lr.y;
}
#define iSampleRate ctx.samplerate
