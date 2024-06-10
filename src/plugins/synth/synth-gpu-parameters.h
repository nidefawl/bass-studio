#include <cstddef>

namespace PluginSynth::GPU {

enum {
    MAX_SYNTH_PARAMS = 256,
    MAX_PARAMS_PER_ADSR = 32,
    MAX_PARAMS_PER_LFO = 16,
    MAX_ADSR_LFO = 8
};

enum {
    NUM_AUDIO_CHANNELS = 2,
    NUM_POLY_VOICES    = 24,
    MAX_UNISON_VOICES  = 16,
};

/* keep in sync with shader defines */
enum {
    NUM_VOICE_INPUT_PARAMETERS = 5,
    NUM_SYNTH_INPUT_PARAMETERS = 2,
};

enum ParametersSynthGPU : size_t {
    MasterVolume = 0,
    VoiceMode,
    Osc1Gain,
    Osc1Waveform,
    Osc1UnisonVoiceCount,
    Osc1UnisonDetune,
    Osc1Filter,
    Osc1KeytrackFilter,
    Osc1KeytrackDetune,
    Osc1KeytrackStereoWidth,
    Osc1Coarse,
    Osc1Fine,
    Osc1Stereo,
    Osc1PulseWidth,
    Osc1PulseWidthModDepth,
    Osc1PulseWidthModRate,
    ADSR_1_A_Duration = MAX_SYNTH_PARAMS,
    ADSR_1_H_Duration,
    ADSR_1_D_Duration,
    ADSR_1_S_Amount,
    ADSR_1_R_Duration,
    ADSR_1_A_Shape,
    ADSR_1_D_Shape,
    ADSR_1_R_Shape,
    ADSR_2_A_Duration = MAX_SYNTH_PARAMS + MAX_PARAMS_PER_ADSR,
    ADSR_2_H_Duration,
    ADSR_2_D_Duration,
    ADSR_2_S_Amount,
    ADSR_2_R_Duration,
    ADSR_2_A_Shape,
    ADSR_2_D_Shape,
    ADSR_2_R_Shape,
    LFO_1_Frequency = MAX_SYNTH_PARAMS + MAX_ADSR_LFO * MAX_PARAMS_PER_ADSR,
    LFO_1_TriggerMode, // 0 = note resets phase, 1 = one shot (note resets phase), 2 = song position
    LFO_1_Phase,
    LFO_1_RampDuration,
    LFO_2_Frequency = MAX_SYNTH_PARAMS + MAX_ADSR_LFO * MAX_PARAMS_PER_ADSR + MAX_PARAMS_PER_LFO,
    LFO_2_TriggerMode,
    LFO_2_Phase,
    LFO_2_RampDuration,
};

} // namespace PluginSynth::GPU
