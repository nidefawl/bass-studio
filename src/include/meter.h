#pragma once

#include "audioblock.h"
#include "seq_math.h"
#include "seq_util.h"
#include "seq_time.h"

//TODO: make samplerate dependent
#define RUNNING_SUM_BUF_SIZE (1024*1)
template <uint32_t N>
class runningsum {
public:

	double runningSum = 0;
	float rsBuffer[N] = {0};
	int rsIdx = 0;
	float fMax = 0;
	float fPeak = 0;
	float fLvl = 0;
	float fPeakFalloffDelay = 0;
	void update(float* fBuf, uint32_t samples) {
		uint32_t i;
		float fMaxBlock = 0.0f;
		for (i = 0; i < samples; i++) {
			float f = *fBuf;
			f = f * f;
			fMaxBlock = max(fMaxBlock, f);
			runningSum += f;
			runningSum -= rsBuffer[rsIdx];
			rsBuffer[rsIdx] = f;
			rsIdx++;
			if (rsIdx >= (int32_t)N) {
				rsIdx = 0;
			}
			fBuf++;
		}
		if (fMaxBlock > F_MIN) {
			fMax = max(sqrtf(fMaxBlock), fMax);
		}
		if (fMax > fPeak) {
			fPeak = fMax;
			fPeakFalloffDelay = 2.0f;
		}
		fLvl = runningSum > F_MIN ? (float) sqrt(runningSum / (double) N) : 0.0f;
	}
	void onTick(double since) {
		if (fMax > F_MIN) {
			fMax = max(0.0f, fMax*powf(10.0f, (float)-since));
		} else {
			fMax = 0.0f;
		}
		if (fPeakFalloffDelay > 0) {
			fPeakFalloffDelay -= since;
		} else {
			if (fMax > F_MIN) {
				fPeak = max(0.0f, fPeak*powf(10.0f, (float)-since));
			} else {
				fPeak = 0.0f;
			}
		}

	}

};
template <uint32_t N>
class rmsmeter {
public:
	runningsum<N> channels[2];
	void update(AudioBlock* block) {
		for (uint32_t i = 0; i < min(block->channels, 2u); i++) {
			channels[i].update(block->buf[i], block->samples);
		}
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
		for (uint32_t i = 0; i < 2; i++) {
			channels[i].onTick(since);
		}
	}
};
