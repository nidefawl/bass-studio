#version 430
/* These macros are set by the preprocessor */
#define N_PROGRAM 0
#define N_CHANNELS 0
#define N_SAMPLES 0 // must not exceed 1024
#define N_POLY_VOICES 0
#define N_UNISON_VOICES 0
#define IS_WAVEFORM_SAMPLER 0

/* These macros are to be kept in sync with c++ */
#define NUM_VOICE_INPUT_PARAMETERS 3
#define NUM_SYNTH_INPUT_PARAMETERS 2

/* define any number of programs */
#define PROGRAM_NAME_EMPTY "Empty"
#define PROGRAM_NAME_ANALOG_SAW "Analog-Saw"
#define PROGRAM_NAME_SAW "Saw"
#define PROGRAM_NAME_SQUARE "PWM Square"
#define PROGRAM_NAME_TRIANGLE "Triangle"
#define PROGRAM_NAME_WEIRD_TRI "Weird-Tri"
#define PROGRAM_NAME_SINE "Sine"

#define M_PI 3.1415926538

#if N_SAMPLES < 1
    #error "N_SAMPLES must be defined"
#endif
#if N_POLY_VOICES < 1
    #error "N_POLY_VOICES must be defined"
#endif
#if N_UNISON_VOICES < 1
    #error "N_UNISON_VOICES must be defined"
#endif

struct voice_state_input_t {
    float velocity[N_SAMPLES];
    float pitch[N_SAMPLES];
    float param_filter[N_SAMPLES];
};

struct synth_state_input_t {
    float param_filter_keytrack[N_SAMPLES];
    float param_stereo[N_SAMPLES];
};

layout(std430, binding = 0) buffer block_in_synth_state
{
    synth_state_input_t synth;
} state_in_synth;

layout(std430, binding = 1) buffer block_in_voice_state
{
    voice_state_input_t voices[];
} state_in;
layout(std430, binding = 2) buffer block_out_audio
{
    float samples[2*N_SAMPLES];
} audioblock_out;
layout(std430, binding = 3) buffer block_out_waveform
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
    double osc1_unison_voice_count;
    double osc1_unison_detune;
    double osc1_filter;
    double osc1_stereo;
    double osc1_pw;
    double osc1_pw_mod_rate;
    double osc1_pw_mod_depth;
    double osc1_filter_keytrack;
    double osc1_detune_keytrack;
    double osc1_width_keytrack;
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

double saw(double phase, double phase_inc, double bleb) {
    // return ( p)*2.0 - 1.0;
    double saw = fract(phase);
    double fsaw = 1.0 - 2.0 * saw;
    double dura = bleb * 4.0;
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
double s(double fshape) {
    return 1.0-pow(float(1.0-fshape), 4.0);
}
double square(double phase, double phase_inc, double bleb, double pw) {
    const double PW = pw;
    double f = fract(phase);
    double square = f < PW ? 1.0 : -1.0;
    double dura = bleb * 4.0;
    if (f < phase_inc*dura) {
        square *= s(f / (phase_inc*dura));
    }
    if (f >= 1.0 - phase_inc*dura) {
        square *= s((1.0 - f) / (phase_inc*dura));
    }
    // same at PW (0.5)
    if (f >= PW - phase_inc*dura && f < PW) {
        square *= s((PW - f) / (phase_inc*dura));
    }
    if (f >= PW && f < PW + phase_inc*dura) {
        square *= s((f - PW) / (phase_inc*dura));
    }
    return square;
}

double osc_pulsewidth(double pw, double t, double phase_offset) {
    double f = pow(2.0, float(ctx.osc1_pw_mod_rate) * 21.0) * 0.01;
    double pw_mod = pw + 0.5 * ctx.osc1_pw_mod_depth * sin(float(2.0 * M_PI * (t * f + phase_offset)));
    return clamp(pw_mod, 0.15, 0.85);
}

double s2(double tri, double fshape) {
    return pow(float(fshape), 4.0);
}

double triangle(double phase, double phase_inc, double bleb) {
    // double tri = abs(float(fract(phase)-0.5)) * 4.0 - 1.0;
    double f = fract(phase);
    double tri = f < 0.5 ? f*4.0-1.0 : (1.0-f)*4.0-1.0;
    double f_sin = double(sin(float(f*2.0*M_PI - M_PI*0.5)));
    // if (f < 0.5) {
    //     return f_sin;
    // }
    double dura = bleb * 1.0;
    if (f < phase_inc*dura) {
        double f2 = 1.0 - (f / (phase_inc*dura));
        // fade to sin 
        tri = mix(tri, f_sin, f2 * f2);
    }
    if (f >= 1.0 - phase_inc*dura) {
        double f2 = 1.0 - ((1.0 - f) / (phase_inc*dura));
        // fade to sin 
        tri = mix(tri, f_sin, f2 * f2);
    }
    if (f >= 0.5 - phase_inc*dura && f < 0.5) {
        double f2 = 1.0 - ((0.5 - f) / (phase_inc*dura));
        // fade to sin 
        tri = mix(tri, f_sin, f2 * f2);
    }
    if (f >= 0.5 && f < 0.5 + phase_inc*dura) {
        double f2 = 1.0 - ((f - 0.5) / (phase_inc*dura));
        // fade to sin 
        tri = mix(tri, f_sin, f2 * f2);
    }
    return tri;
}

// this is not a triangle, but nice anyway
double weird_tri(double phase, double phase_inc, double bleb) {
    // double tri = abs(float(fract(phase)-0.5)) * 4.0 - 1.0;
    double f = fract(phase);
    double tri = f < 0.5 ? f*4.0-1.0 : (1.0-f)*4.0-1.0;
    double dura = 16.0+bleb * 4.0;
    if (f < phase_inc*dura) {
        tri *= s(f / (phase_inc*dura));
    }
    if (f >= 1.0 - phase_inc*dura) {
        tri *= s((1.0 - f) / (phase_inc*dura));
    }
    tri -= 0.12;
    return tri;
}

float shapeSegment(float t, float shape) {
    float shapeBi  = 1.0 - shape * 2.0;
    float shapeExp = 0.0;
    float scale2   = 0.2 + t * 0.8;
    if (shapeBi < 0.0) {
        shapeExp = 1.0 + scale2 * abs(shapeBi) * 16.0;
    } else {
        shapeExp = 1.0 / (1.0 + scale2 * abs(shapeBi) * 16.0);
    }
    return pow(t, shapeExp);
}

double sine(double phase, double phase_inc, double bleb) {
    float fsin = sin(float(fract(phase) * 2.0 * M_PI));
    float b = float(bleb*(1.0/128.0));
    b = 1.0 - b;
    b = b * b;
    b = 1.0 - b;
    return double(shapeSegment(abs(fsin), b) * sign(fsin) * (1.0 - 0.01));
}

double saw_analog(double phase, double phase_inc, double bleb) {
    double saw = 2.0 * (sin(float(fract(phase)*1.5705))-0.5);
    saw = -saw;
    // same as saw, but with a different shape
    double dura = bleb * 4.0;
    if (fract(phase) < phase_inc*dura) {
        double fshape = fract(phase) / (phase_inc*dura);
        saw *= s(fshape);
    }
    if (fract(phase) >= 1.0 - phase_inc*dura) {
        double fshape = (1.0 - fract(phase)) / (phase_inc*dura);
        saw *= s(fshape);
    }
    // saw += double (bleb * (1.0/128.0) * -0.3);
    return saw;
}

double waveform_mix(double phase, double phase_inc, double bleb) {
    // each cycle is a different waveform
    double idx = phase / 1.0;
    switch (int(idx) % 4) {
        case 0: return saw(phase, phase_inc, bleb);
        case 1: return square(phase, phase_inc, bleb, 0.5);
        case 2: return sine(phase, phase_inc, bleb);
        case 3: return triangle(phase+0.25, phase_inc, bleb);
    }
    return 0.0;
}



void processSynthUnison()
{
    const uint i = gl_LocalInvocationIndex.x;
    const double sample_offset = ctx.time_samples + double(i)/*  - ctx.time_sample_phase_reset */;
    const float t = float(sample_offset * ctx.one_over_samplerate);
    const int VC = int(round(ctx.osc1_unison_voice_count));
    vec2 s_lr = vec2(0.0);
    float uv_gain_range = 0.4;
    float uv_gain_adj = VC < 2 ? 1.5 : 1.0 / sqrt(float(VC));
    float seed = 2.0;
    for (int j = 0; j < N_POLY_VOICES; j++)
    {
        if (state_in.voices[j].velocity[i] >= 0.0)
        {
            const float a = state_in.voices[j].velocity[i];
            const float f = state_in.voices[j].pitch[i]; // hz
            const double pitchLogScale = double(log2(f / 440.0)) + 2.0;
            double osc1_det_keytrack_bipolar = ctx.osc1_detune_keytrack * 2.0 - 1.0;
            double osc1_detune = ctx.osc1_unison_detune * (1.0 + clamp((pitchLogScale + 2.0)*0.66, -1.0, 1.0) * osc1_det_keytrack_bipolar);
            double osc1_flt_keytrack_bipolar = state_in_synth.synth.param_filter_keytrack[i] * 2.0 - 1.0;
            double filter_keytrack = 1.0 + clamp((pitchLogScale-1.0) * -0.8, -1.0, 1.0) * osc1_flt_keytrack_bipolar;
            double stereo_width = state_in_synth.synth.param_stereo[i];
            stereo_width += (0.0+clamp((pitchLogScale+1.0)*0.25, -1.0, 1.0) * (ctx.osc1_width_keytrack * 2.0 - 1.0));
            stereo_width = clamp(stereo_width, 0.0, 1.0);
            for (int u = 0; u < N_UNISON_VOICES && u < VC; ++u)
            {
                float uv_index = VC <= 1 ? 0.5 : float(u) / float(VC-1);
                float uv_index_centered = uv_index * 2.0 - 1.0;
                double uv_f = double(f) - double(abs(uv_index_centered)) * osc1_detune;
                float uv_p = noise2(j + seed, u*N_POLY_VOICES+j);
                if (uv_p < 0.0) {
                    uv_p = -uv_p;
                }
                double phase_inc = uv_f * ctx.one_over_samplerate;
                double p = phase_inc * sample_offset + double(uv_p);
                float panLR = (pow(abs(uv_index_centered), 2.0) * sign(uv_index_centered) * float(stereo_width) + 1.0) * 0.5;
                vec2 pan_lr = normalize(mix(vec2(1.0, 0.0), vec2(0.0, 1.0), panLR));
                double param_filter = state_in.voices[j].param_filter[i] * max(filter_keytrack, 0.02);
                // param_filter = max(0.0, param_filter * 128.0);
                param_filter = max(0.0, (pow(float(param_filter), 1.5)) * 128.0);
#if N_PROGRAM == PROGRAM_SAW
                s_lr += float(saw(p, phase_inc, param_filter)) * a * pan_lr;
#elif N_PROGRAM == PROGRAM_SINE
                s_lr += float(sine(p, phase_inc, 0.25*param_filter)) * a * pan_lr;
#elif N_PROGRAM == PROGRAM_SQUARE
                double uv_square_pw = osc_pulsewidth(ctx.osc1_pw, t, uv_p*222.34567);
                s_lr += float(square(p, phase_inc, param_filter, uv_square_pw)) * a * pan_lr;
#elif N_PROGRAM == PROGRAM_TRIANGLE
                s_lr += float(triangle(p, phase_inc, param_filter)) * a * pan_lr;
#elif N_PROGRAM == PROGRAM_WEIRD_TRI
                s_lr += float(weird_tri(p, phase_inc, param_filter)) * a * pan_lr;
#elif N_PROGRAM == PROGRAM_ANALOG_SAW
                s_lr += float(saw_analog(p, phase_inc, param_filter)) * a * pan_lr;
#endif
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
    float cycles = 6.0;
    float phase = i*cycles / float(N_SAMPLES) + 0.5;
    float phase_inc = cycles / float(N_SAMPLES);
    double param_filter = (1.0 - ctx.osc1_filter) * 128.0;
#if N_PROGRAM == PROGRAM_SAW
    double s = saw(phase, phase_inc, param_filter);
#elif N_PROGRAM == PROGRAM_SINE
    double s = sine(phase, phase_inc, 0.25*param_filter);
#elif N_PROGRAM == PROGRAM_SQUARE
    float t = float(ctx.time_samples * ctx.one_over_samplerate);
    double uv_square_pw = osc_pulsewidth(ctx.osc1_pw, t, 0);
    double s = square(phase, phase_inc, param_filter, uv_square_pw);
#elif N_PROGRAM == PROGRAM_TRIANGLE
    double s = triangle(phase, phase_inc, param_filter);
#elif N_PROGRAM == PROGRAM_WEIRD_TRI
    double s = weird_tri(phase, phase_inc, param_filter);
#elif N_PROGRAM == PROGRAM_ANALOG_SAW
    double s = saw_analog(phase, phase_inc, param_filter);
#else
    double s = 0.0;
#endif
    waveform_out.samples[i] = float(s);
}

#if N_PROGRAM != PROGRAM_EMPTY
void main() {
#if IS_WAVEFORM_SAMPLER == 1
    sampleWaveform();
#else
    processSynthUnison();
#endif
}
#else
void main() {
    audioblock_out.samples[gl_LocalInvocationIndex.x] = 0.0;
    audioblock_out.samples[gl_LocalInvocationIndex.x + N_SAMPLES] = 0.0;
}
#endif
