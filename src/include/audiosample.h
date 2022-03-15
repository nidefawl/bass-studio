#pragma once
#include <vector>
#include "types.h"
#include "str_util.h"
#include "samplerate.h"

using samplechannel_t = std::vector<float>;
struct audiosample_t {
    channelnum_t nChannels;
    samplecount_t nSamples;
    samplerate_t sampleRate;
    uint16_t bitsPerSample;
    std::vector<samplechannel_t> samples;
    std::vector<std::vector<samplechannel_t>> downsampled;
};

struct samplesource_t {
    virtual audiosample_t* getSample() = 0;
    virtual ~samplesource_t()          = default;
};
