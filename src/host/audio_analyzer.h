#pragma once
#include <vector>
#include <memory>
#include "dsp/fft.h"
#include "platform.h"
#include "host/audiohost/audio_host.h"

using fft_processor_lf = fft_processor<512 * 8, 4>;
using fft_processor_hf = fft_processor<512 * 4, 2>;

class audioanaylzer {
public:
    double timeSwitchEffect = 0;
    std::unique_ptr<fft_processor_hf> analyzerHf;
    std::unique_ptr<fft_processor_lf> analyzerLf;
    int64_t nSamples     = 0;
    double processedTime = 0.0;


    int64_t lastFrameTime = 0;
    double timeReload     = 0;
    int64_t tLast         = 0;
    bool processInput     = true;
    audioanaylzer() : tLast(getTimeMicros()) {
    }
    void init(blocksize_t _blockSize, samplerate_t _sampleRate) {
        analyzerHf = std::make_unique<fft_processor_hf>(_blockSize, _sampleRate);
        analyzerLf = std::make_unique<fft_processor_lf>(_blockSize, _sampleRate);
    }
    void onTick() {
        int64_t tNow   = getTimeMicros();
        int64_t tSince = tNow - tLast;
        if (tSince >= 5000) {
            double tickSince = tSince / 1000000.0;
            analyzerLf->onTick(tickSince);
            analyzerHf->onTick(tickSince);
            timeReload += tickSince;
            if (timeReload >= 30.0) {
                timeReload = 0;
            }
            tLast = tNow;
        }
    }
    void processBlock(AudioBlock* buf, float fGain) {
        if (this->processInput && buf->samples == analyzerHf->blocksize) {
            analyzerHf->processBuffer(buf, fGain);
            analyzerLf->processBuffer(buf, fGain);
        }
        nSamples += buf->samples;
        processedTime = nSamples / (double) analyzerHf->samplerate;
    }
};
