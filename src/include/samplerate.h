#pragma once
#include <stdint.h>
#include "str_util.h"
#include "seq_util.h"
#include "assert_dbg.h"

using samplerate_t = uint32_t;
#define SAMPLERATE_T_MAX_VALUE_U32 ((samplerate_t)(0xFFFFFFFFUL))
#define INVALID_SAMPLE_OFFSET_U32 ((samplerate_t)(0xFFFFFFFFUL))

enum class sampleformat_bits_t : int32_t {
    NONE 		= 0,
    FLOAT_32 	= 1 << 0,
    FLOAT_64 	= 1 << 1,
};
struct sampleformat_t {
	samplerate_t sampleRate;
	int32_t blockSize;
	sampleformat_bits_t sampleformat = sampleformat_bits_t::NONE;
};
inline const char* sampleformat_bits_to_str(sampleformat_bits_t t) {
	if (t == sampleformat_bits_t::FLOAT_32)
		return "FLOAT_32";
	if (t == sampleformat_bits_t::FLOAT_64)
		return "FLOAT_64";
	return "NONE";
}
namespace DAW {
inline samplerate_t NumSamplesResampled(samplerate_t numSamples, samplerate_t sampleRateA, samplerate_t sampleRateB) {
	uint64_t n = numSamples;
	n = n * sampleRateB/sampleRateA;
	dbgassert(FitsTypeRange<samplerate_t>(n));
	return static_cast<samplerate_t>(n);
}
}
inline bool operator==(sampleformat_t const& a, sampleformat_t const& b) {
	return a.blockSize == b.blockSize && a.sampleRate == b.sampleRate && a.sampleformat == b.sampleformat;
}
inline bool operator!=(sampleformat_t const& a, sampleformat_t const& b) {
	return !(a == b);
}
