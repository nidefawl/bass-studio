#pragma once
#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <chrono>

using hp_clock = std::chrono::high_resolution_clock;
#define LOG(fmtString,...) printf(fmtString "\n", ##__VA_ARGS__); fflush(stdout)
class test_rng {
	uint16_t lfsr = 0xACE1u;
	uint32_t bit;
public:
	uint32_t randI() {

		bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
		return lfsr = (lfsr >> 1) | (bit << 15);
	}
	uint32_t randInt(int range) {
		return randI() % range;
	}
};
