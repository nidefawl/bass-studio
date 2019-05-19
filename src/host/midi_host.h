#pragma once
#include <stdint.h>
#include "config.h"
#include "samplerate.h"
#include "str_util.h"
#include "seq_time.h"
#include "midi-msg.h"

class project_controller_t;
typedef void PmStream;
typedef void PmQueue;
typedef int32_t PmMessage;
class midihost {
private:
	/* shared queues */
	std::vector<MidiIOEvent> midiMsgsIn;
	std::vector<MidiIOEvent> midiMsgsOut;
	std::atomic<PmStream*> streamIn{NULL};
	std::atomic<PmStream*> streamOut{NULL};
	bool pmIsInitalized = false;
	//	audiothread_ringbuffer_t ringbuffer;
	bool enableProcessing = true;
	bool inputInSysex = false;
	bool outputInSysex = false;
	int32_t last_timestamp = 0;
	void handleMessage(PmMessage, std::vector<MidiIOEvent>& messages);
public:
	midihost();
	static midihost* getInstance();
//	void enqueue(AudioBuffer*);
	int32_t processMidi(project_controller_t* ctrl, int32_t sample, double posDouble, playback_state state, bool inLoop, bool isLoopAround);
	bool hasInputMessages() {
		return midiMsgsIn.size() > 0;
	}
	std::vector<MidiIOEvent> getInputMessages() {
		std::vector<MidiIOEvent> ret = midiMsgsIn;
		std::sort(ret.begin(), ret.end(), [](auto& a, auto& b){
			return a.timestamp < b.timestamp;
		});
		midiMsgsIn.clear();
		return ret;
	}
	bool initPm();
	void deinitPm();
	void onStreamEnd();
	bool startMidi();
	bool stopMidi();
	bool isStreaming() {
		return this->streamIn != NULL || this->streamOut != NULL;
	}
};

