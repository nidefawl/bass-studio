#pragma once
#include "types.h"


class seq_rand {
    uint64_t rng_state = (uint64_t) this;

public:
    uint32_t rng_bits(uint8_t nBits) {
        return rng_rand() & ((1 << nBits) - 1);
    }
    uint32_t rng_rand(uint32_t max) {
        return max == 0 ? 0 : rng_rand() % static_cast<uint64_t>(max);
    }
    uint32_t rng_rand() {
        uint32_t xorshifted = static_cast<uint32_t>(((rng_state >> 18u) ^ rng_state) >> 27u);
        uint32_t rot        = rng_state >> 59u;
        rng_state           = rng_state * 6364136223846793005ULL + 1;
        return (xorshifted >> rot) | (xorshifted << ((-(int) rot) & 31));
    }
    double rng_double() {
        return (rng_rand()&0xFFFFFFFF) / (static_cast<double>(0xFFFFFFFF) + 1.0);
    }
    void rng_seed(uint64_t seed) {
        seed *= 708169373ULL;
        rng_state = ((seed + 632191u) * 6343u) | 1u;
    }
};
