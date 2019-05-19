#pragma once
#include <stdint.h>
#include "config.h"
#include "samplerate.h"
#include "str_util.h"
#include "seq_time.h"
#include "dsp_util.h"
//#include "project.h"
#include "audiobuffer.h"
#include "../util/readerwriterqueue.h"


typedef void PaStream;
class audiohost {
private:
	moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueue;
	std::atomic<PaStream*> stream{NULL};
	bool paIsInitalized = false;
	//	audiothread_ringbuffer_t ringbuffer;
public:
	uint32_t blockReads = 0;
	uint32_t bufferUnderuns = 0;
	samplerate_t lSampleRate;
	uint16_t lBlockSize;
	uint8_t numChannels;
private:
public:
	audiohost(uint32_t _sampleRate, uint16_t _blockSize);
	static audiohost* getInstance();
	void enqueue(AudioBuffer*);
	bool try_dequeue(AudioBuffer*&);
	bool initPa();
	void deinitPa();
	void onStreamEnd();
	bool startAudio();
	bool stopAudio();
	bool isStreaming() {
		return this->stream != NULL;
	}
};

