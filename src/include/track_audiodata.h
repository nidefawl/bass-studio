#pragma once
#include "config.h"
#include <vector>
#include <memory>
#include "automation.h"
#include "audioblock.h"
#include "samplerate.h"
#include "note.h"
#include "leak_detect.h"

struct VstEvent_t;

//TODO: make samplerate dependent
#define RUNNING_SUM_BUF_SIZE (1024*1)
template <uint32_t N>
class runningsum {
public:

	double runningSum = 0;
	float rsBuffer[N] = {0};
	int rsIdx = 0;
	float fMax = 0;
	float fPeak = 0;
	float fLvl = 0;
	float fPeakFalloffDelay = 0;
	void update(float* fBuf, uint32_t samples) {
		uint32_t i;
		float fMaxBlock = 0.0f;
		for (i = 0; i < samples; i++) {
			float f = *fBuf;
			f = f * f;
			fMaxBlock = max(fMaxBlock, f);
			runningSum += f;
			runningSum -= rsBuffer[rsIdx];
			rsBuffer[rsIdx] = f;
			rsIdx++;
			if (rsIdx >= N) {
				rsIdx = 0;
			}
			fBuf++;
		}
		if (fMaxBlock > F_MIN) {
			fMax = max(sqrtf(fMaxBlock), fMax);
		}
		if (fMax > fPeak) {
			fPeak = fMax;
			fPeakFalloffDelay = 2.0f;
		}
		fLvl = runningSum > F_MIN ? (float) sqrt(runningSum / (double) N) : 0.0f;
	}
	void onTick(double since) {
		if (fMax > F_MIN) {
			fMax = max(0.0f, fMax*powf(10.0f, (float)-since));
		} else {
			fMax = 0.0f;
		}
		if (fPeakFalloffDelay > 0) {
			fPeakFalloffDelay -= since;
		} else {
			if (fMax > F_MIN) {
				fPeak = max(0.0f, fPeak*powf(10.0f, (float)-since));
			} else {
				fPeak = 0.0f;
			}
		}

	}

};
template <uint32_t N>
class rmsmeter {
public:
	runningsum<N> channels[2];
	void update(AudioBlock* block) {
		for (uint32_t i = 0; i < min(block->channels, 2u); i++) {
			channels[i].update(block->buf[i], block->samples);
		}
	}
	float getRms(int i) {
		return channels[i].fLvl;
	}
	float getMax(int i) {
		return channels[i].fMax;
	}
	float getStandingPeak(int i) {
		return channels[i].fPeak;
	}
	void onTick(double since) {
		for (uint32_t i = 0; i < 2; i++) {
			channels[i].onTick(since);
		}
	}
};
class vstplugin;

struct trackparam_automation_t : public automation_t {
	float& gain;
	trackparam_automation_t(float& _gain) : automation_t(), gain(_gain) {

	}
	float getDstValue() override {
		return gain;
	}
	void setDstValue(float f) override {
		gain = f;
	}
};
struct track_mixer: public automatable_t {
	float gain;
	trackparam_automation_t gainAutomation;
	track_mixer() : automatable_t(), gainAutomation(gain) {

	}
	String getAutomatableName() override {
		return "Mixer";
	}
	int32_t getNumParameters() override {
		return 1;
	}
	String getParamName(int32_t paramIdx) override {
		return "Gain";
	}
	float getParamValue(int32_t idx) override {
		return gain;
	}
	void setParamValue(int32_t idx, float val) override {
		gain = val;
	}
	void updateAutomatedParameters(tick_t pos) override {
//		float val = param.src->getValueAt(pos);
//		setParamValue(param.paramIdx, val);
	}
	automation_t* getAutomation(int32_t idx) override {
		return &gainAutomation;
	}
};
struct track_plugins_t {
	track_t* const track;
	const samplerate_t& sampleRate;
	const uint16_t& blockSize;
//	float level = 0;
	rmsmeter<16000> meter;
	vstplugin* instrument = NULL;
	std::vector<vstplugin*> effects;
	std::vector<note_t> heldNotes;
	VstEvent_t* midiEventsBuf = NULL;
	AudioBlock input; //guaranteed to have at least 2 channels
	AudioBlock output; //guaranteed to have at least 2 channels
	track_mixer mixer;
	automatable_t* selectedAutomationCtr = NULL;
	int32_t selectedAutomationParam = -1;
	track_plugins_t(track_t* _track, const samplerate_t& _sampleRate, const uint16_t& _blockSize, int32_t nChannels)
	: track(_track),
	  sampleRate(_sampleRate),
	  blockSize(_blockSize), input(nChannels, _blockSize), output(nChannels, _blockSize), mixer() {
	}
	~track_plugins_t();
	vstplugin* setInstrument(vstplugin* _instrument);
	void removePlugin(vstplugin* _vst);
	void insertEffect(int32_t idx, vstplugin* _instrument);
	void sendNotesOff(int32_t bpm100, int32_t blockSamplePos);
	void sendNotes(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos);
	void onTick(double since);
	VstEvent_t* reallocEvts(size_t size);
	void getAutomatableTargets(std::vector<automatable_t*>& targets);
};
