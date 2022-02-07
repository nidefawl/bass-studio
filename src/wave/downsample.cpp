#include <algorithm>
#include <soxr.h>
#include <vector>
#include "assert_dbg.h"
#include "samplerate.h"

int downsample(samplerate_t sampleRate, float* samplesIn, int64_t numSamples, std::vector<float>& samplesOut, uint8_t downsampleBits) {

    //Straight forward downsampling using internal LPF.I can't remember any details about this
    //
    //        float srtDown = sampleRate / (float) (1 << downsampleBits);
    ////double ft = (srtDown*0.4f);
    //double ft     = (sampleRate * 0.45f);
    //double bt     = 8000 / (float) (1 << downsampleBits);
    //double ripple = 0.001;
    //int lenCoeffs;
    //double* coeff      = calcLPF(sampleRate, ft, ripple, bt, &lenCoeffs);
    //int nStep          = 1 << downsampleBits;
    //int lenSamplesDown = len >> downsampleBits;
    //for (int j = 0; j < lenSamplesDown; j++) {
    //    int pos   = j * nStep;
    //    float out = 0.0;
    //    for (int y = 0; y < lenCoeffs; y++) {
    //        out += samplesIn[math::max(0, pos)] * coeff[y];
    //        pos--;
    //    }
    //    samplesOut[j] = out;
    //}

    soxr_io_spec_t iospec;
    iospec.flags = 0;
    iospec.scale = 1;
    iospec.e     = 0;
    iospec.itype = SOXR_FLOAT32_I;
    iospec.otype = SOXR_FLOAT32_I;

    size_t odone = 0;

    double orate = static_cast<double>(sampleRate) / (1 << downsampleBits);

    auto ilen = static_cast<size_t>(numSamples);
    auto olen = static_cast<size_t>(numSamples * orate / static_cast<double>(sampleRate) + .5); /* Assay output len. */
    samplesOut.resize(olen);
    soxr_error_t error = soxr_oneshot(sampleRate, orate, 1,            /* Rates and # of chans. */
                                      samplesIn, ilen, NULL,            /* Input. */
                                      samplesOut.data(), olen, &odone, /* Output. */
                                      &iospec, NULL, NULL);            /* Default configuration.*/

    dbgassert(!error);
    dbgassert(odone <= (samplesOut.size()));

    return (int) (int64_t) (error);// soxr_error_t is const char*
}
