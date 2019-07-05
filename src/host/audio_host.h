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
#include "meter.h"


typedef void PaStream;
class audiohost {
public:
	using meterType = rmsmeter<16000, 1>;
	struct audiostream {
		enum StreamDirection {
			DIR_IN = 1,
			DIR_OUT = 2,
		};
		int streamId = 1;
		audiohost* host{NULL};
		PaStream* stream{NULL};
		int flagsIO = DIR_IN | DIR_OUT;
		int idx = 0;
		int32_t nInputChannels = 0;
		int32_t nOutputChannels = 0;
		String inputName;
		String outputName;
		String device_api;
		std::atomic<bool> streamShouldEnd{false};
		std::atomic<bool> streamFinished{false};
		moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueue;
		moodycamel::ReaderWriterQueue<AudioBuffer*> audioQueueInput;
		audiothread_ringbuffer_t ringbuffer;
		std::vector<std::shared_ptr<meterType>> metersInput;
		std::vector<std::shared_ptr<meterType>> metersOutput;
		audiostream(int32_t streamId, int32_t nOutputChannels = 0, int32_t nInputChannels = 0);
		~audiostream();
		audiothread_ringbuffer_t& getRingbuffer() {
			return ringbuffer;
		}
		void enqueue(AudioBuffer*);
		bool try_dequeue(AudioBuffer*&);
		void enqueueInput(AudioBuffer*);
		bool try_dequeueInput(AudioBuffer*&);
		int32_t getOutputQueueSize() {
			return audioQueue.size_approx();
		}
		int32_t getInputQueueSize() {
			return audioQueueInput.size_approx();
		}
	};
private:
	std::vector<std::shared_ptr<audiostream>> streams;
	bool paIsInitalized = false;
	//	audiothread_ringbuffer_t ringbuffer;
public:
	uint32_t blockReads = 0;
	uint32_t bufferUnderuns = 0;
	samplerate_t lSampleRate;
	uint16_t lBlockSize;
	uint8_t numChannels;
	int32_t nextStreamId;
private:
public:
	audiohost(uint32_t _sampleRate, uint16_t _blockSize);
	~audiohost();
	static audiohost* getInstance();
	audiostream* getStream(int idx);
	bool initPa();
	void deinitPa();
	void removeStream(audiostream* stream);
public:
	bool startAudio();
	bool stopAudio();
	bool isStreaming() {
		return !streams.empty();
	}
};

