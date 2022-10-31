#pragma once
#include "types.h"
#include <vector>
#include <array>
#include "math/vec.h"
#include "audiosample.h"

class clip_t;
class waveformrender;
struct NVGLUframebuffer;

enum SampleMethod {
    sample_clip,
    sample_straight,
    sample_energy
};

struct audioclip_texture_t {
    ivec2 pos{};
    ivec2 size{};
    samplecount_t sampleBegin = 0;
    samplecount_t sampleBeginOffset = 0;
    samplecount_t sampleEnd = 0;
    audioclip_loop_pos_t loopPos;
    double samplesPerPx    = 0;
    int quality            = 1;
    float scaleX           = 1.0f;
    float scaleY           = 1.0f;
    float linewidth        = 1.0f;
    SampleMethod method    = SampleMethod::sample_straight;
    int64_t audioId        = -1;
    int64_t sampleVersion  = -1;
    bool clipped           = false;
    float posPreLoopEnd = 0.0f;
    std::array<sample_fades_t,2> fades{};
};


inline bool isEqualWaveform3(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) {
    if (lhs.audioId != rhs.audioId)
        return false;
    if (lhs.sampleVersion != rhs.sampleVersion)
        return false;
    if ((lhs.sampleBeginOffset - lhs.sampleBegin) != (rhs.sampleBeginOffset - rhs.sampleBegin)) {
        return false;
    }
    if ((lhs.sampleEnd - lhs.sampleBegin) != (rhs.sampleEnd - rhs.sampleBegin)) {
        return false;
    }
    if (lhs.clipped == rhs.clipped && lhs.quality == rhs.quality && lhs.method == rhs.method) {
        if (lhs.fades != rhs.fades)
            return false;
        if (lhs.loopPos != rhs.loopPos)
            return false;
    }
    return false;
}


inline bool operator==(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) {
    return lhs.audioId == rhs.audioId &&
           lhs.sampleVersion == rhs.sampleVersion &&
           lhs.pos == rhs.pos &&
           //lhs.startOffset == rhs.startOffset &&
           lhs.size == rhs.size &&
           lhs.method == rhs.method &&
           lhs.clipped == rhs.clipped &&
           lhs.quality == rhs.quality &&
           lhs.sampleBegin == rhs.sampleBegin &&
           lhs.sampleBeginOffset == rhs.sampleBeginOffset &&
           lhs.sampleEnd == rhs.sampleEnd &&
           lhs.samplesPerPx == rhs.samplesPerPx &&
           lhs.fades == rhs.fades &&
           lhs.loopPos == rhs.loopPos;
}

inline bool operator!=(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) { return !operator==(lhs, rhs); }

