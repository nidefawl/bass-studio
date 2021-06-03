#pragma once
#include <stdint.h>
#include <vector>
#include <array>
#include <algorithm>
#include <iterator>
#include "assert_dbg.h"
#include "note.h"
#include "seq_time.h"
#include "str_util.h"
#include "logging.h"
#include "platform.h"
#include "automation.h"
#include "track_impl.h"


struct arp_snapshot;

class midiarp : public automatable_t {
public:
	enum ResetMode : int {
		NOTE, BEAT
	};
private:
	struct arp_param_entry_t {
		int32_t id = 0;
		String name;
		float val = 0.0f;
	};
//	struct arp_pattern_entry_t {
//		std::vector<int32_t> pattern;
//	};
#define ARP_PARAM_CLOCK PARAM_OFFSET_IMPL
#define ARP_PARAM_GATE (PARAM_OFFSET_IMPL+1)
#define ARP_PARAM_PATTERN (PARAM_OFFSET_IMPL+2)
//	const std::array<std::vector<int32_t>, 4> patterns { {
//		{0, 1, 0, 2, 0, 3, 0, 4, 0 },
//		{8, 7, 6, 5, 4, 3, 2, 1, 0 },
//		{0, 1, 2, 3, 4, 3, 2, 1, 0 },
//	} };
	ResetMode resetMode = ResetMode::NOTE;
	std::vector<noteevent_t> heldInput;
	std::vector<noteevent_t> heldOutput;
public:
	std::vector<note_t> heldOutputNotes;
	std::vector<note_t> heldInputAnimationNotes;
	std::vector<note_t> heldOutputAnimationNotes;
	std::vector<marker_t> markers;
	std::vector<int64_t> notesSpawnTime;
private:
#define NUM_ARP_STEPSIZE_OPTIONS 16
#define NUM_PATTERNS 6
	int32_t step = 0;
	tick_t resetTime = 0;
	tick_t lastStepSize = 0;
	int noteIdx = 0;
	std::array<tick_t, 16*3> tickLength;
	int32_t numCalls = 0;
	track_impl_t* const trackImpl;
public:
	bool enable = false;
	midiarp(track_impl_t* _trImpl) : automatable_t(), trackImpl(_trImpl) {
		for (int i = 0; i < NUM_ARP_STEPSIZE_OPTIONS; i += 2) {
			tickLength[i + 0] = (TICKS_16TH >> 3) << (i >> 1);
			tickLength[i + 1] = tickLength[i + 0] + (tickLength[i + 0] >> 1);
			dbgassert(tickLength[i + 0] > 0);
		}
		const std::array<arp_param_entry_t, 5> parameterTypes { {
			arp_param_entry_t{PARAM_ENABLE, "Enabled", 0.0f},
			arp_param_entry_t{PARAM_GAIN, "Gain", 1.0f},
			arp_param_entry_t{ARP_PARAM_CLOCK, "Clock", 10.0f/(float)NUM_ARP_STEPSIZE_OPTIONS},
			arp_param_entry_t{ARP_PARAM_GATE, "Gate", 1/4.0f},
			arp_param_entry_t{ARP_PARAM_PATTERN, "Pattern", 0.0f},
		} };
		for (const arp_param_entry_t& paramEntry : parameterTypes) {
			automatable_param_t* regparam = registerParam(paramEntry.id);
			regparam->value = paramEntry.val;
			regparam->label = paramEntry.name;
			regparam->shortLabel = paramEntry.name;
		}
//		setParamValue(ARP_PARAM_ENABLED, 0, FLG_PAR_UPDATE_INIT);
//		setParamValue(ARP_PARAM_CLOCK, 10/(double)NUM_ARP_STEPSIZE_OPTIONS, FLG_PAR_UPDATE_INIT);
//		setParamValue(ARP_PARAM_GATE, 1/4.0f, FLG_PAR_UPDATE_INIT);
//		setParamValue(ARP_PARAM_PATTERN, 0, FLG_PAR_UPDATE_INIT);
		getOrCreateAutomation(PARAM_ENABLE)->quantizationSteps = 1;
		getOrCreateAutomation(ARP_PARAM_CLOCK)->quantizationSteps = NUM_ARP_STEPSIZE_OPTIONS-1;
	}
	~midiarp() {

	}
	void reset(tick_t _resetTime) {
		noteIdx = 0;
		resetTime = _resetTime;
		step = 0;
	}
	void allNotesOff() {
		heldInput.clear();
		heldOutput.clear();
		heldOutputNotes.clear();
		heldInputAnimationNotes.clear();
		markers.clear();
	}
	float getGateF() {
		return getParamValue(ARP_PARAM_GATE);
	}
	float getPatternF() {
		return getParamValue(ARP_PARAM_PATTERN);
	}
	float getClockF() {
		return getParamValue(ARP_PARAM_CLOCK);
	}
	float getGainF() {
		return getParamValue(PARAM_GAIN);
	}
	tick_t getStepSize() {
		int32_t option = (int32_t)std::floor(getClockF()*(NUM_ARP_STEPSIZE_OPTIONS-1));
		dbgassert(option<NUM_ARP_STEPSIZE_OPTIONS);
		int32_t len = tickLength[option];
		dbgassert(len>0);
		return len;
	}
	tick_t getDuration() {
		const int minDuration = getStepSize()>>3;
		const int maxDuration = getStepSize()<<1;
		tick_t len = (tick_t)std::floor(minDuration+getGateF()*(maxDuration-minDuration));
		dbgassert(len>0);
		return len;
	}
	int isChordOutput() {
		int32_t option = (int32_t) std::floor(getPatternF() * (NUM_PATTERNS - 1));
		dbgassert(option<NUM_PATTERNS);
		return option == 0;
	}
	int getStepIdx(int step, int nNotes) {
		int32_t option = (int32_t) std::floor(getPatternF() * (NUM_PATTERNS - 1));
		dbgassert(option<NUM_PATTERNS);
		if (option > 0) {
			option--;
		}
		switch (option) {
		case 0:
			return step%nNotes;
		case 1:
			return nNotes-1-(step%nNotes);
		case 2:
			return (step/nNotes)%2 == 0 ? step%nNotes : (nNotes-1-(step%nNotes));
		case 3:
			return (step/nNotes)%2 == 0 ? (nNotes-1-(step%nNotes)) : step%nNotes;
		case 4:
			return step%2 == 0 ? 0 : (step/2)%nNotes;
		}
		return 0;
	}

	void addNote(std::vector<noteevent_t>& noteEvents, tick_t start, note_t& note, int64_t time) {
		int32_t noteVelocity = math::clamp<int32_t>(this->getGainF() * note.velocity, 0, 127);
		noteEvents.emplace_back(note.pitch, noteVelocity, note.start()-start, true, false);
		heldOutputNotes.push_back(note);
		heldOutputAnimationNotes.push_back(note);
		notesSpawnTime.push_back(time);
		dbgassert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
	}


	String getAutomatableName() override {
		return "Arp";
	}
	float getParamValue(int32_t idx) override {
		automatable_param_t* param = getParam(idx);
		dbgassert(param);
		return param->value;
	}
	void onEnable() {

	}
	void onDisable() {

	}
	void setParamValue(int32_t idx, float val, int flags) override {
		automatable_param_t* param = getParam(idx);
		dbgassert(param);
		param->value = val;
		if (param->idx == PARAM_ENABLE) {
			bool wasEnable = this->enable;
			this->enable = val > 0;
			if (this->enable != wasEnable) {
				if (this->enable) {
					onEnable();
				} else {
					onDisable();
				}
				if (!(flags & FLG_PAR_UPDATE_INIT)) {
					param->inUse = true;
				}
			}
		}
	}
	automationlane_snapshot_t toRef() override {
		automationlane_snapshot_t ref;
		ref.type = AUTOMATABLE_ARP;
		ref.refId = 0;
		return ref;
	}
	track_t* getTrack() override {
		dbgassert(this->trackImpl);
		return this->trackImpl->getTrack();
	}
	void createSnapshot(arp_snapshot& snapshot);
	void loadSnapshot(const arp_snapshot& snapshot);

	void process(std::vector<noteevent_t>& noteEventsIn,
					tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd,
					std::vector<noteevent_t>& noteEventsProcessed);
	int writeOutputNotes(std::vector<noteevent_t>& noteEventsProcessed,
			tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int64_t time);
	void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
};
