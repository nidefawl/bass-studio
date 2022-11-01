#include <algorithm>
#include <soxr.h>
#include <vector>
#include "assert_dbg.h"
#include "dsp/CalcKaiserWindow.h"
#include "logging.h"
#include "math/seq_math.h"
#include "samplerate.h"
#include "types.h"

namespace {
    constexpr bool useSoxrDownsample = false;
}

int downsample(samplerate_t sampleRate, float* samplesIn, samplecount_t offset, samplecount_t numSamples, std::vector<float>& samplesOut, uint8_t downsampleBits) {
    if (useSoxrDownsample) {
        soxr_io_spec_t iospec;
        iospec.flags = 0;
        iospec.scale = 1;
        iospec.e     = 0;
        iospec.itype = SOXR_FLOAT32_I;
        iospec.otype = SOXR_FLOAT32_I;

        size_t odone = 0;

        double orate = static_cast<double>(sampleRate) / (1 << downsampleBits);

        auto ilen = static_cast<size_t>(numSamples);
        auto olen = static_cast<size_t>((numSamples) * orate / static_cast<double>(sampleRate) + .5); /* Assay output len. */
        auto offsetDown = static_cast<size_t>(offset * orate / static_cast<double>(sampleRate) + .5); /* Assay output len. */
        if (samplesOut.size() < olen) {
            samplesOut.resize(olen);
        }
        soxr_error_t error = soxr_oneshot(sampleRate, orate, 1,            /* Rates and # of chans. */
                                        samplesIn + offset, ilen, NULL,            /* Input. */
                                        samplesOut.data() + offsetDown, olen, &odone, /* Output. */
                                        &iospec, NULL, NULL);            /* Default configuration.*/

        dbgassert(!error);
        dbgassert(odone <= (samplesOut.size()));

        return (int) (int64_t) (error);// soxr_error_t is const char*
    } else {
        static thread_local int lenCoeffs = 0;
        static thread_local samplerate_t lastSampleRate = 0;
        static thread_local double* coeffs = nullptr;
        if (coeffs == nullptr || sampleRate != lastSampleRate) {
            //Straight forward downsampling using internal LPF. I can't remember any details about this
            const double ft     = (sampleRate * 0.45f);
            const double bt     = 20000 / (float) (1 << downsampleBits);
            const double ripple = 0.001;
	        std::free(coeffs);
            coeffs = calcLPF(sampleRate, ft, ripple, bt, &lenCoeffs);
            lastSampleRate = sampleRate;
        }
        samplecount_t nStep          = 1 << downsampleBits;
        samplecount_t lenSamplesDown = numSamples >> downsampleBits;
        if (samplesOut.size() < size_t(lenSamplesDown)) {
            samplesOut.resize(lenSamplesDown);
        }
        for (int j = 0; j < lenSamplesDown; j++) {
            samplecount_t pos   = j * nStep;
            float out = 0.0;
            for (int y = 0; y < lenCoeffs; y++) {
                out += samplesIn[math::max<samplecount_t>(0, pos)] * float(coeffs[y]);
                pos--;
            }
            samplesOut[j] = out;
        }
        return 0;
    }
}
