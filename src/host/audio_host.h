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
#include "audio_config.h"
#include "appsettings.h"


typedef void PaStream;
class audiohost {
public:
	struct audiostream {
		struct audiotrack {
			audiotrack(int32_t _index, AudioIO::tracktype _type, int32_t _channelOffset, runningsum<16000>* sums)
			: meter(sums, AudioIO::getNumChannelsTrackType(_type)), buf((uint32_t)AudioIO::getNumChannelsTrackType(_type), 0),
			  index(_index),
			  channelOffset(_channelOffset),
			  type(_type) {

			}
			~audiotrack() {

			}
			rmsmeter<16000> meter;
			AudioBlock buf;
			int32_t index = 0;
			int32_t channelOffset = 0;
			AudioIO::tracktype type;
		};
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
		rmsmeterimpl<16000, 32> metersInput;
		rmsmeterimpl<16000, 32> metersOutput;
		std::vector<std::shared_ptr<audiotrack>> tracksInput;
		std::vector<std::shared_ptr<audiotrack>> tracksOutput;
		audiostream(int32_t streamId, AudioIO::io_cfg_tracks cfg, int32_t nOutputChannels = 0, int32_t nInputChannels = 0);
		~audiostream();
		audiothread_ringbuffer_t& getRingbuffer() {
			return ringbuffer;
		}
		static inline String getTrackName(audiotrack* track, bool isInput) {
			return AudioIO::getTrackName(track->type, track->index, isInput);
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
	uint32_t inputBufferUnderuns = 0;
	int32_t audioCallbackInvocationDelay_usec = 0;
	samplerate_t lSampleRate = 0;
	uint16_t lBlockSize = 0;
	int32_t nextStreamId;
private:
public:
	audiohost();
	~audiohost();
	static audiohost* getInstance();
	audiostream* getStream(int idx);
	bool initPa();
	void deinitPa();
	void removeStream(audiostream* stream);
public:
	bool startAudio(app_iosettings& settings);
	bool stopAudio();
	bool isStreaming() {
		return !streams.empty();
	}
};

