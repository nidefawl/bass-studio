#pragma once
#include "types.hpp"
#include "seq_util.hpp"
#include "assert_dbg.h"

#define SAMPLERATE_T_MAX_VALUE_U32 ((samplerate_t) (0xFFFFFFFFUL))
#define INVALID_SAMPLE_OFFSET_U32 ((samplecount_t) (0xFFFFFFFFUL))

enum class sampleformat_bits_t : int32_t {
    NONE     = 0,
    FLOAT_32 = 1 << 0,
    FLOAT_64 = 1 << 1,
};
struct sampleformat_t {
    samplerate_t sampleRate = 0;
    blocksize_t blockSize = 0;
    sampleformat_bits_t sampleformat = sampleformat_bits_t::FLOAT_32;
};
inline const char* sampleformat_bits_to_str(sampleformat_bits_t t) {
    if (t == sampleformat_bits_t::FLOAT_32)
        return "FLOAT_32";
    if (t == sampleformat_bits_t::FLOAT_64)
        return "FLOAT_64";
    return "NONE";
}
namespace DAW {
    inline samplecount_t NumSamplesResampled(samplecount_t numSamples, samplerate_t sampleRateA, samplerate_t sampleRateB) {
        uint64_t n = numSamples;
        n = n * sampleRateB / sampleRateA;
        return static_cast<samplecount_t>(n);
    }
}// namespace DAW
inline bool operator==(sampleformat_t const& a, sampleformat_t const& b) {
    return a.blockSize == b.blockSize && a.sampleRate == b.sampleRate && a.sampleformat == b.sampleformat;
}
inline bool operator!=(sampleformat_t const& a, sampleformat_t const& b) {
    return !(a == b);
}
