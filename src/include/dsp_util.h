#pragma once
#include <stdint.h>
#include "samplerate.h"

namespace dsp_util {

	float Saturate(float input, float fMax);
	void fillSaturate(float** buffer, uint32_t samples);
	void fillSine(float** buffer, uint32_t samples);
	void fillNoise(float** buffer, uint32_t samples);
	void fillSqare(samplerate_t samplerate, float freq, float** buffer, uint32_t samples);
	void fillSilence(float** buffer, uint32_t samples);
	void copyBuffer(float** dst, float** src, uint32_t samples);
	float clampGain(float f);
	float clampReadGain(float f);
	float dBFS(float f);
	float dBFSClampInf6(float f);
	float fromdBFSClampInf6(float f);
	float fromdBFS(float f);
	const float DBFS_FLOOR = -80.0f;
	const float DBFS_MUTE_POS = -81.0f;
	const float DBFS_INF_POS = -100.0f;
	const float MTR_FLOOR = -48.0f;
	const float MTR_CEIL = 6.0f;
	extern const float GAIN_DB6;
	extern const float GAIN_DBFLOOR;
	extern const float GAIN_DBINF;
	float scaledRange(float db, float lvlFloor, float lvlCeil);

}
