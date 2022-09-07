#pragma once
#include <vector>
#include "samplerate.h"

/**
* Invoke soxr to downsample single channel audio data
* @param sampleRate
* @param samplesIn
* @param len
* @param samplesOut
* @param downSampleFactor
* @return 0 = no error
*/
int downsample(samplerate_t sampleRate, float* samplesIn, samplecount_t offset, samplecount_t numSamples, std::vector<float>& samplesOut, uint8_t downSampleFactor);
