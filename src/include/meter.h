#pragma once
#include <stdint.h>
#include "math/seq_math.h"
#include "audioblock.h"
#include "seq_util.h"
#include "seq_time.h"

//TODO: make samplerate dependent
struct meter_lvls {
	float fMax = 0;
	float fPeak = 0;
	float fLvl = 0;
};
#define RUNNING_SUM_BUF_SIZE (1024*1)
template <uint32_t N>
class runningsum {
public:
	float rsBuffer[N] = {0};
	double runningSum = 0;
	int rsIdx = 0;
	float fMax = 0;
	float fPeak = 0;
	float fLvl = 0;
	float fPeakFalloffDelay = 0;
	void update(const float* fBuf, uint32_t samples, float fGain) {
		//TODO: find out if this could be done more efficiently. Think about SIMD or look at the assembly
		uint32_t i;
		float fMaxBlock = 0.0f;
		for (i = 0; i < samples; i++) {
			float f = *fBuf;
			f *= fGain;
			f = f * f;
			fMaxBlock = math::max(fMaxBlock, f);
			runningSum += f;
			runningSum -= rsBuffer[rsIdx];
			rsBuffer[rsIdx] = f;
			rsIdx++;
			if (rsIdx >= (int32_t)N) {
				rsIdx = 0;
			}
			fBuf++;
		}
		if (fMaxBlock > math::F_MIN) {
			fMax = math::max(sqrtf(fMaxBlock), fMax);
		}
		if (fMax > fPeak) {
			fPeak = fMax;
			fPeakFalloffDelay = 2.0f;
		}
		fLvl = runningSum > math::F_MIN ? (float) sqrt(runningSum / (double) N) : 0.0f;
	}
	void onTick(double since) {
		/*
		 * TODO: parameter since is constant on calls from audio thread (blocksize/samplerate)
		 * make the decay curve a state or parameter
		 */
		//
		float decayCurve = math::powf(10.0f, (float)-since);
		if (fMax > math::F_MIN) {
			fMax = math::max(0.0f, fMax*decayCurve);
		}
		if (!(fMax > math::F_MIN)) {
			fMax = 0.0f;
		}
		if (fPeakFalloffDelay > 0) {
			fPeakFalloffDelay -= since;
		} else {
			if (fPeak > math::F_MIN) {
				fPeak = math::max(0.0f, fPeak*decayCurve);
			}
			if (!(fPeak > math::F_MIN)) {
				fPeak = 0.0f;
			}
		}

	}
	meter_lvls getLevels() {
		return meter_lvls{fMax, fPeak, fLvl};
	}
};

template <uint32_t N>
struct rmsmeter {
	runningsum<N>* channels;
	int32_t numChannels;
	rmsmeter(runningsum<N>* _channels, int32_t _numChannels)
	: channels(_channels), numChannels(_numChannels) {

	}
	float getRms(int i) {
		return channels[i].fLvl;
	}
	float getMax(int i) {
		return channels[i].fMax;
	}
	float getStandingPeak(int i) {
		return channels[i].fPeak;
	}
	void onTick(double since) {
		for (decltype(numChannels) i = 0; i < numChannels; i++) {
			channels[i].onTick(since);
		}
	}
	std::vector<meter_lvls> getLevels() {
		std::vector<meter_lvls> v;
		for (decltype(numChannels) i = 0; i < numChannels; i++) {
			v.push_back(std::move(channels[i].getLevels()));
		}
		return v;
	}
};
template <uint32_t N, uint32_t C = 2>
class rmsmeterimpl {
public:
	runningsum<N> channels[C];
	void update(const AudioBlock* block, float fTrackGain) {
		for (uint32_t i = 0; i < math::min(block->channels, C); i++) {
			channels[i].update(block->buf[i], block->samples, fTrackGain);
		}
	}
	float getMaxRMS() {
		float f = 0.0f;
		if (C) {
			f = channels[0].fLvl;
			for (auto& cn : channels) {
				f = std::max(f, cn.fLvl);
			}
		}
		return f;
	}
	float getMaxPeak() {
		float f = 0.0f;
		if (C) {
			f = channels[0].fMax;
			for (auto& cn : channels) {
				f = std::max(f, cn.fMax);
			}
		}
		return f;
	}
	float getRms(int i) {
		return channels[i].fLvl;
	}
	float getMax(int i) {
		return channels[i].fMax;
	}
	float getStandingPeak(int i) {
		return channels[i].fPeak;
	}
	void onTick(double since) {
		for (uint32_t i = 0; i < C; i++) {
			channels[i].onTick(since);
		}
	}
	std::vector<meter_lvls> getLevels() {
		std::vector<meter_lvls> v;
		for (uint32_t i = 0; i < C; i++) {
			v.push_back(std::move(channels[i].getLevels()));
		}
		return v;
	}
};
