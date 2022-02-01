#include "fft.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <array>
#include <memory>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <cstdlib>
#include <tools/kiss_fftr.h>


void applyWindowAndPadding(float* in, size_t inLen, std::vector<float>& windowedPadded, int32_t fftlen, float fGain) {
    memset(windowedPadded.data(), 0, windowedPadded.size() * sizeof(float));
    for (int i = 0; i < inLen; i++) {
        double multiplier = (1.0 - cos(2.0 * M_PI * i / (double)(inLen - 1)));
        windowedPadded[i] = multiplier * in[i] * fGain;
    }
}
void fillbands(std::vector<float> const& mags, std::vector<float> const& freq, std::vector<float>& bands, int32_t fftlen,
               double srOverFFT) {
    std::vector<float> newBands(freq.size());
    int numBands = freq.size();
    for (int i = 0; i < numBands; i++) {
        double f = freq[i] / srOverFFT;

        int binIdx  = std::floor(f);
        float lower = 0;
        float upper = 0;
        if (binIdx > 0) {
            lower = mags[binIdx];
        }
        if (binIdx + 1 <= fftlen / 2 + 1) {
            upper = mags[binIdx + 1];
        }
        float delta  = f - binIdx;
        float interp = lower + (upper - lower) * delta;
        newBands[i]  = interp;
        if (i < 10) {
            //      my_printf("Band #%d is %f * [%d] + %f * [%d]\n", i, delta, binIdx, 1.0f-delta, binIdx+1);
        }
    }
    for (int i = 0; i < numBands; i++) {
        //    bands[i] = bands[i] * 0.5 + newBands[i] * 0.5;
        bands[i] = bands[i] * 0.15 + newBands[i] * 0.85;

        //    bands[i] = newBands[i];

        //    bands[i] *= 0.9 - ((numBands-i)/(float)numBands)*0.23;
        //    bands[i] = std::max<float>(bands[i], newBands[i]);
    }
}
