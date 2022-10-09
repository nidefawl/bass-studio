#pragma once
#include <vector>
#include "shape.h"
#include "types.h"
#include "str_util.h"
#include "samplerate.h"

struct sample_fades_t {
    DAW::Shape::shape_t shape{};
    samplecount_t samplesFadePos = 0;
    samplecount_t samplesFadeDuration = 0;
};
struct sample_fades_ref_t {
    const DAW::Shape::shape_t* shape{};
    samplecount_t samplesFadePos = 0;
    samplecount_t samplesFadeDuration = 0;
    bool hasFade() const {
        return shape && samplesFadeDuration > 0;
    }
};
bool operator==(const sample_fades_t& lhs, const sample_fades_t& rhs);

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
