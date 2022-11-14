#include "resampler.h"

bool oversampler_t::runResample(AudioBlock& srcBlock, AudioBlock& dstBlock, uint32_t& nOutputProcessed) {
    dbgassert(srcBlock.samples == this->numSamplesInput);
    dbgassert(srcBlock.channels >= this->numChannels);
    dbgassert(dstBlock.samples >= this->numSamplesResampled);
    dbgassert(dstBlock.channels >= this->numChannels);

    for (channelnum_t i = 0; i < numChannels; i++) {
        if (i < srcBlock.channels) {
            channelPtrsIn[i] = srcBlock.buf[i];
        } else {
            channelPtrsIn[i] = nullptr;
        }
        if (i < dstBlock.channels) {
            channelPtrsOut[i] = dstBlock.buf[i];
        } else {
            channelPtrsOut[i] = nullptr;
        }
    }
    if (soxr) {
        size_t outputProcessed = 0;
        soxrError  = soxr_process(soxr, channelPtrsIn.data(), numSamplesInput, nullptr, channelPtrsOut.data(), dstBlock.samples, &outputProcessed);
        this->sampleDelay = soxr_delay(soxr);
        // log_lf(Log::L_TRACE, "soxr_process: %s, delay: %f, outputProcessed: %zu\n", soxr_strerror(soxrError), sampleDelay, outputProcessed);
        if (!soxrError) {
            nOutputProcessed = static_cast<uint32_t>(outputProcessed);
            return outputProcessed > 0;
        }
        log_lf(Log::L_ERROR, "soxr_process failed: %s\n", soxr_strerror(soxrError));
    }
    return false;
}
