#pragma once
#include <cstdint>
#include <vector>
#include "math/vec.h"
#include "audiosample.h"

enum SampleMethod {
    sample_straight,
    sample_energy
};

struct NVGLUframebuffer;
struct audioclip_texture_t {
    ivec2 pos{ 0 };
    ivec2 size{ 0 };
    int64_t sampleBegin{ 0 };
    int64_t sampleBeginOffset{ 0 };
    int64_t sampleEnd{ 0 };
    double samplesPerPx = 0;
    int quality         = 1;
    float scaleX        = 1.0f;
    float scaleY        = 1.0f;
    float linewidth     = 1.0f;
    SampleMethod method = SampleMethod::sample_straight;
    int audioId         = -1;
    bool clipped        = false;
};

inline bool isEqualWaveform3(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) {
    if (lhs.audioId != rhs.audioId)
        return false;
    if ((lhs.sampleBeginOffset - lhs.sampleBegin) != (rhs.sampleBeginOffset - rhs.sampleBegin)) {
        return false;
    }
    if ((lhs.sampleEnd - lhs.sampleBegin) != (rhs.sampleEnd - rhs.sampleBegin)) {
        return false;
    }
    return lhs.clipped == rhs.clipped && lhs.quality == rhs.quality && lhs.method == rhs.method;
}


inline bool operator==(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) {
    return lhs.pos == rhs.pos &&
           //			lhs.startOffset == rhs.startOffset &&
           lhs.size == rhs.size && lhs.sampleBegin == rhs.sampleBegin &&
           lhs.sampleBeginOffset == rhs.sampleBeginOffset && lhs.sampleEnd == rhs.sampleEnd &&
           lhs.samplesPerPx == rhs.samplesPerPx &&
           lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method && lhs.clipped == rhs.clipped;
}

inline bool operator!=(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) { return !operator==(lhs, rhs); }

