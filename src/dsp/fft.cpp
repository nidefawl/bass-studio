#include "fft.hpp"
#include "math/seq_math.hpp"
#include <cmath>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <cstdlib>
#include <kissfft/kiss_fftr.h>

void applyWindowAndPadding(float* in, samplecount_t inLen, std::vector<float>& windowedPadded, samplecount_t fftlen, float fGain) {
    memset(windowedPadded.data(), 0, windowedPadded.size() * sizeof(float));
    for (samplecount_t i = 0; i < inLen; i++) {
        double multiplier = (1.0 - cos(2.0 * M_PI * i / (double)(inLen - 1)));
        windowedPadded[i] = multiplier * in[i] * fGain;
    }
}

void fillbands(std::vector<float> const& mags, std::vector<float> const& freq, std::vector<float>& bands, samplecount_t fftlen, double srOverFFT, float smoothingFactor) {
    static DAW_CXX_CONSTINIT thread_local std::vector<float> newBands;
    newBands.resize(bands.size());
    auto numBands = freq.size();
    
    // For real FFT, we have (fftlen/2 + 1) bins
    const size_t numFFTBins = fftlen / 2 + 1;
    
    for (size_t i = 0; i < numBands; i++) {
        auto f = freq[i] / srOverFFT;
        int binIdx = math::floorfS32(f);
        float lower = 0;
        float upper = 0;
        
        // Better bounds checking
        if (binIdx >= 0 && binIdx < static_cast<int>(numFFTBins)) {
            lower = mags[binIdx];
        }
        if (binIdx + 1 >= 0 && binIdx + 1 < static_cast<int>(numFFTBins)) {
            upper = mags[binIdx + 1];
        }
        
        float delta = f - binIdx;
        float interp = lower + (upper - lower) * delta;
        newBands[i] = interp;
    }
    
    for (size_t i = 0; i < numBands; i++) {
        // Use configurable smoothing factor
        bands[i] = bands[i] * (1.0f - smoothingFactor) + newBands[i] * smoothingFactor;
    }
}