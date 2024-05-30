#version 430
#define N_PROGRAM 0
#define N_CHANNELS 0
#define N_SAMPLES 0
#define NUM_POLY_VOICES 0
#define NUM_UNISON_VOICES 0
#define M_PI 3.1415926538

#if N_SAMPLES < 1
    #error "N_SAMPLES must be defined"
#endif
#if NUM_POLY_VOICES < 1
    #error "NUM_POLY_VOICES must be defined"
#endif
#if NUM_UNISON_VOICES < 1
    #error "NUM_UNISON_VOICES must be defined"
#endif

struct voice_state_input_t {
    float is_active[N_SAMPLES];
    float velocity[N_SAMPLES];
    float pitch[N_SAMPLES];
};

layout(std430, binding = 0) buffer block_in
{
    voice_state_input_t voices[];
} state_in;

layout(std430, binding = 1) buffer block_out_audio
{
    float samples[2*N_SAMPLES];
} audioblock_out;

layout(std430, binding = 0) buffer block_out_waveform
{
    float samples[];
} waveform_out;

layout(binding = 0) uniform context {
    double one_over_samplerate;
    double time_sample_phase_reset;
    double bpm;
    double time_seconds;
    double time_samples;
    double time_beats;
    double unison_voice_count;
    double unison_detune;
    double bleb_duration;
} ctx; 

layout(local_size_x = N_SAMPLES, local_size_y = 1, local_size_z = 1) in;

//mini
float noise1(float seed1,float seed2){
    return(
        fract(seed1+12.34567*
        fract(100.*(abs(seed1*0.91)+seed2+94.68)*
        fract((abs(seed2*0.41)+45.46)*
        fract((abs(seed2)+757.21)*
        fract(seed1*0.0171))))))
        * 1.0038 - 0.00185;
}

//2 seeds
float noise2(float seed1,float seed2){
    float buff1 = abs(seed1+100.94) + 1000.;
    float buff2 = abs(seed2+100.73) + 1000.;
    buff1 = (buff1*fract(buff2*fract(buff1*fract(buff2*0.63))));
    buff2 = (buff2*fract(buff2*fract(buff1+buff2*fract(seed1*0.79))));
    buff1 = noise1(buff1, buff2);
    return(buff1 * 1.0038 - 0.00185);
}

double saw(double phase, double phase_inc) {
    // return ( p)*2.0 - 1.0;
    double saw = fract(phase);
    double fsaw = 1.0 - 2.0 * saw;
    double dura = ctx.bleb_duration * 4.0;
    if (saw < phase_inc*dura) {
        double fshape = saw / (phase_inc*dura);
        // smoothsteppy shape:
        // fshape = fshape * fshape * (3.0 - 2.0 * fshape);
        // fshape = 1.0-pow(1.0-fshape, max(dura*0.1, 0.0001));
        fshape = 1.0-pow(float(1.0-fshape), 4.0);
        fsaw *= fshape;
    }
    if (saw >= 1.0 - phase_inc*dura) {
        double fshape = (1.0 - saw) / (phase_inc*dura);
        // smoothsteppy shape:
        // fshape = 1.0 - fshape;
        // fshape = fshape * fshape * (3.0 - 2.0 * fshape);
        fshape = 1.0-pow(float(1.0-fshape), 4.0);
        // fshape = 1.0-pow(1.0-fshape, max(dura*0.05, 0.0001));
        // fshape = 1.0 - fshape;
        fsaw *= fshape;
    }
    return fsaw;
}

#if 0
// layout(local_size_x = NUM_POLY_VOICES*NUM_UNISON_VOICES, local_size_y = 1, local_size_z = 1) in;
void processSynthUnisonSamplesOuter()
{
    const int VC = int(round(ctx.unison_voice_count));
    float vs = VC / 256.0;
    float uv_gain_range = 0.2;
    float uv_gain_adj = VC < 2 ? 3.0 : pow(1.0-vs, 3.0) * (1.0 - uv_gain_range) + uv_gain_range;
    // const uint j = gl_LocalInvocationID.x;
    // const uint u = gl_LocalInvocationID.y;
    const uint j = gl_LocalInvocationIndex.x / NUM_UNISON_VOICES;
    const uint u = gl_LocalInvocationIndex.x % NUM_UNISON_VOICES;
    const float uv_index = VC <= 1 ? 0.5 : float(u) / float(VC-1);
    const float uv_p = noise2(j, u*NUM_POLY_VOICES+j);
    const vec2 pan_lr = mix(vec2(1.0, 0.0), vec2(0.0, 1.0), uv_index);
    vec2 s_lr = vec2(0.0);
    const uint i = gl_WorkGroupID.x;
    {
        const double sample_offset = ctx.time_samples + double(i) - ctx.time_sample_phase_reset;
        // const float t = float(sample_offset * ctx.one_over_samplerate);
        if (state_in.voices[j].is_active[i] > 0.0)
        {
            const float f = state_in.voices[j].pitch[i]; // hz
            const float a = state_in.voices[j].velocity[i];
            const double octave = double(floor(log2(f / 440.0))) - 3.0;
            const double unison_det_scale_note = clamp((octave*-0.3), 0.0, 1.0) * 0.5 + 0.5;
            double uv_f = double(f) - double(uv_index) * ctx.unison_detune * unison_det_scale_note;
            if (u%2 == 1 && state_in.voices[j].pitch[i] > 330) {
                // uv_f *= 0.5;
            }
            double phase_inc = uv_f * ctx.one_over_samplerate;
            double p = phase_inc * sample_offset + double(uv_p);
            // vec2 pan_lr = vec2(sqrt(uv_index), sqrt(1.0 - uv_index));
            s_lr += float(saw(p, phase_inc)) * a * pan_lr;
        }
        s_lr *= uv_gain_adj;
        // s_lr.x += noise2(t, t) * 0.05;
    }
    audioblock_out.samples[i] = s_lr.x;
    audioblock_out.samples[i + N_SAMPLES] = s_lr.y;

    if (i == 0) {
        audioblock_out.samples[0] = gl_LocalInvocationIndex.x;
    }
    // audioblock_out.samples[N_SAMPLES] = s_lr.y;
}
#endif

void processSynthUnison()
{
    const uint i = gl_LocalInvocationIndex.x;
    const double sample_offset = ctx.time_samples + double(i) - ctx.time_sample_phase_reset;
    const float t = float(sample_offset * ctx.one_over_samplerate);
    const int VC = int(round(ctx.unison_voice_count));
    vec2 s_lr = vec2(0.0);
    float vs = VC / 256.0;
    float uv_gain_range = 0.2;
    float uv_gain_adj = VC < 2 ? 3.0 : pow(1.0-vs, 3.0) * (1.0 - uv_gain_range) + uv_gain_range;
    for (int j = 0; j < NUM_POLY_VOICES; j++)
    {
        if (state_in.voices[j].is_active[i] > 0.0)
        {
            const float f = state_in.voices[j].pitch[i]; // hz
            const float a = state_in.voices[j].velocity[i];
            const double octave = double(floor(log2(f / 440.0))) - 3.0;
            const double unison_det_scale_note = clamp((octave*-0.3), 0.0, 1.0) * 0.5 + 0.5;
            for (int u = 0; u < VC; ++u)
            {
                float uv_index = VC <= 1 ? 0.5 : float(u) / float(VC-1);
                double uv_f = double(f) - double(uv_index) * ctx.unison_detune * unison_det_scale_note;
                if (u%2 == 1 && state_in.voices[j].pitch[i] > 330) {
                    // uv_f *= 0.5;
                }
                float uv_p = noise2(j, u*NUM_POLY_VOICES+j);
                double phase_inc = uv_f * ctx.one_over_samplerate;
                double p = phase_inc * sample_offset + double(uv_p);
                // vec2 pan_lr = vec2(sqrt(uv_index), sqrt(1.0 - uv_index));
                vec2 pan_lr = mix(vec2(1.0, 0.0), vec2(0.0, 1.0), uv_index);
                s_lr += float(saw(p, phase_inc)) * a * pan_lr;
            }
        }
    }
    s_lr *= uv_gain_adj;
    // s_lr.x += noise2(t, t) * 0.05;
    audioblock_out.samples[i] = s_lr.x;
    audioblock_out.samples[i + N_SAMPLES] = s_lr.y;
}


void sampleWaveform()
{
    const uint i = gl_LocalInvocationIndex.x;
    float time_sample = float(i);
    float cycles = 2.0;
    float phase = i*cycles / float(N_SAMPLES) + 0.5;
    float phase_inc = cycles / float(N_SAMPLES);
    double s = saw(phase, phase_inc);
    waveform_out.samples[i] = float(s);
}

void main() {
#if N_PROGRAM == 0
    sampleWaveform();
#elif N_PROGRAM == 1
    processSynthUnison();
#else
    #error "N_PROGRAM must be defined"
#endif
}
