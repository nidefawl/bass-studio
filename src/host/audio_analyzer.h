#pragma once
#include <vector>
#include <memory>
#include "dsp/fft.h"
#include "platform.h"
#include "audio_host.h"

using fft_processor_lf = fft_processor<512*8, 4>;
using fft_processor_hf = fft_processor<512*2, 2>;

class audioanaylzer {
public:
	double startOfDay = 0;
	double timeSwitchEffect = 0;
	std::unique_ptr<fft_processor_hf> analyzerHf;
	std::unique_ptr<fft_processor_lf> analyzerLf;
	audiohost* host = nullptr;
	int64_t nSamples = 0;
	double processedTime = 0.0;


	int64_t lastFrameTime = 0;
	double timeReload = 0;
	uint64_t start = 0;
	int64_t tLast = 0;
	bool processInput = true;
	audioanaylzer() :
		tLast(getTimeHPint64()) {
	}
	void init(audiohost* host) {
		assert(host);
		assert(host->lBlockSize > 0);
		assert(host->lSampleRate > 0);
		this->host = host;
		startOfDay = getTimeMillis();
		analyzerHf = std::make_unique<fft_processor_hf>(host->lBlockSize, host->lSampleRate);
		analyzerLf = std::make_unique<fft_processor_lf>(host->lBlockSize, host->lSampleRate);
	}
	void onTick() {

		int64_t tNow = getTimeHPint64();
		int64_t tSince = tNow - tLast;
		if (tSince >= 5000ULL) {
			double tickSince = tSince / 1000000.0;
			analyzerLf->onTick(tickSince);
			analyzerHf->onTick(tickSince);
			timeReload += tickSince;
			if (timeReload >= 30.0) {
				timeReload = 0;
//				if (currentEffect) {
	//				currentEffect->reload();
	//				my_printf("currentEffect->reload\n",0);
//				}
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
		processedTime = nSamples / (double) host->lSampleRate;
	}
};
