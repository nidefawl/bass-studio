#pragma once
#include <vector>
#include "host/shape/shape.h"
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

struct audioclip_loop_pos_t {
    samplecount_t clipSampleOffset = 0;
    samplecount_t loopStart = 0;
    samplecount_t loopEnd = 0;
    samplecount_t preLoopLen = 0;
};
inline bool operator==(const audioclip_loop_pos_t& lhs, const audioclip_loop_pos_t& rhs)
{
    return std::tie(lhs.clipSampleOffset, lhs.loopStart, lhs.loopEnd, lhs.preLoopLen) ==
           std::tie(rhs.clipSampleOffset, rhs.loopStart, rhs.loopEnd, rhs.preLoopLen);
}
inline bool operator<(const audioclip_loop_pos_t& lhs, const audioclip_loop_pos_t& rhs)
{
    return std::tie(lhs.clipSampleOffset, lhs.loopStart, lhs.loopEnd, lhs.preLoopLen) <
           std::tie(rhs.clipSampleOffset, rhs.loopStart, rhs.loopEnd, rhs.preLoopLen);
}

struct clip_audio_settings_t {
    float pitch = 0.0f;
    float stretch = 1.0f;
    auto operator<=>(const clip_audio_settings_t&) const = default;
};

using samplechannel_t = std::vector<float>;
struct audiosample_t {
    channelnum_t nChannels;
    samplecount_t nSamples;
    samplerate_t sampleRate;
    uint16_t bitsPerSample;
    std::vector<samplechannel_t> samples;
    std::vector<std::vector<samplechannel_t>> downsampled;
    int64_t sampleVersion = 0;
    clip_audio_settings_t settings;
};

struct samplesource_t {
    virtual audiosample_t* getSample() = 0;
    virtual ~samplesource_t()          = default;
};

namespace DAW {

struct AudioClipFadeLoopProcessor {
    const sample_fades_ref_t& fadeIn;
    const sample_fades_ref_t& fadeOut;
    const audiosample_t* sample;
    samplecount_t offsetStart;
    samplecount_t preLoopLen;
    samplecount_t loopStart;
    samplecount_t loopEnd;
    float get(channelnum_t ch, samplecount_t samplePos) const;
    float getDownsampled(channelnum_t ch, samplecount_t samplePos, uint8_t nDownLevel, int32_t& status) const;
};

}
