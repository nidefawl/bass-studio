#pragma once
#include <cstdint>
#include <vector>
#include "str_util.h"
#include "samplerate.h"

using samplechannel_t = std::vector<float>;
struct audiosample_t {

    // The sample rate. Will be set to something like 44100.
    samplerate_t sampleRate;

    // The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc.
    uint16_t nChannels;

    // The bits per sample. Will be set to somthing like 16, 24, etc.
    uint16_t bitsPerSample;

    // The total number of samples making up the audio data. Use <totalSampleCount> * <bytesPerSample> to calculate
    // the required size of a buffer to hold the entire audio data.
    int64_t nSamples;

    std::vector<samplechannel_t> samples;//TODO: rename to "channels"
    std::vector<std::vector<samplechannel_t>> downsampled;
};

struct samplesource_t {
    virtual audiosample_t* getSample() = 0;
    virtual ~samplesource_t()          = default;
};
