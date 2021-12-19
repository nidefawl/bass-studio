#pragma once
#include <stdint.h>

class test_rng {
	uint16_t lfsr = 0xACE1u;
	uint32_t bit;
public:
	uint32_t randI() {

		bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
		lfsr = static_cast<uint16_t>((lfsr >> 1) | (bit << 15));
		return lfsr;
	}
	uint32_t randInt(int range) {
		return randI() % static_cast<uint32_t>(range);
	}
};
