#include <algorithm>
#include <soxr.h>
#include <vector>
#include "assert_dbg.h"
#include "dsp/CalcKaiserWindow.h"
#include "logging.h"
#include "math/seq_math.h"
#include "samplerate.h"
#include "types.h"
#include <array>

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
        class FilterCoeffs {
            public:
            struct Filter {
                samplerate_t sampleRate;
                int downsampleBits;
                double* coeffs;
                int lenCoeffs;
            };
            std::vector<Filter> filters;
            Filter* get(samplerate_t sampleRate, int downsampleBits) {
                for (auto& filter : filters) {
                    if (filter.sampleRate == sampleRate && filter.downsampleBits == downsampleBits) {
                        return &filter;
                    }
                }
                filters.push_back({sampleRate, downsampleBits, nullptr, 0});
                auto* filter = &filters.back();

                const double ft     = (filter->sampleRate * 0.45f);
                const double bt     = 20000 / (float) (1 << filter->downsampleBits);
                const double ripple = 0.001;
                filter->coeffs = calcLPF(filter->sampleRate, ft, ripple, bt, &filter->lenCoeffs);
                if (!assert_expr(filter->coeffs)) {
                    filters.pop_back();
                    return nullptr;
                }
        
                return filter;
            };
        };
        static thread_local FilterCoeffs filterCoeffs;
        auto filter = filterCoeffs.get(sampleRate, downsampleBits);
        if (!assert_expr(filter)) {
            return -1;
        }
        samplecount_t nStep          = static_cast<samplecount_t>(1) << downsampleBits;
        samplecount_t lenSamplesDown = numSamples >> downsampleBits;
        if (samplesOut.size() < size_t(lenSamplesDown)) {
            samplesOut.resize(lenSamplesDown);
        }
        for (int j = 0; j < lenSamplesDown; j++) {
            samplecount_t pos   = j * nStep;
            float out = 0.0;
            for (int y = 0; y < filter->lenCoeffs; y++) {
                out += samplesIn[math::max<samplecount_t>(0, pos)] * float(filter->coeffs[y]);
                pos--;
            }
            samplesOut[j] = out;
        }
        return 0;
    }
}
