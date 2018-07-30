#pragma once
#include <stdint.h>
#include <vector>
#include <array>
#include <algorithm>
#include <iterator>
#include <assert.h>
#include "note.h"
#include "seq_time.h"
#include "logging.h"
#include "platform.h"
#include "automation.h"

using std::min;
using std::max;
struct arp_snapshot;

class midiarp : public automatable_t {
public:
	enum ResetMode
		: int {NOTE, BEAT
	};
private:
	struct arp_param_entry_t {
		String name;
		float val;
	};
//	struct arp_pattern_entry_t {
//		std::vector<int32_t> pattern;
//	};
#define ARP_PARAM_ENABLED 0
#define ARP_PARAM_CLOCK 1
#define ARP_PARAM_GATE 2
#define ARP_PARAM_PATTERN 3
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
#define PARAM_ARP_CLOCK 1
#define PARAM_ARP_GATE 2
#define PARAM_ARP_PATTER 3
	int32_t step = 0;
	tick_t resetTime = 0;
	tick_t lastStepSize = 0;
	int noteIdx = 0;
	std::array<tick_t, 16*3> tickLength;
	int32_t numCalls = 0;
public:
	bool enable = false;
	midiarp() : automatable_t() {
		for (int i = 0; i < NUM_ARP_STEPSIZE_OPTIONS; i += 2) {
			tickLength[i + 0] = (TICKS_16TH >> 3) << (i >> 1);
			tickLength[i + 1] = tickLength[i + 0] + (tickLength[i + 0] >> 1);
			assert(tickLength[i + 0] > 0);
		}
		int32_t idx = 0;
		const std::array<arp_param_entry_t, 4> parameterTypes { {
			arp_param_entry_t{"Enabled", 1.0f},
			arp_param_entry_t{"Clock", 1.0f},
			arp_param_entry_t{"Gate", 1.0f},
			arp_param_entry_t{"Pattern", 1.0f},
		} };
		params.reserve(parameterTypes.size());
		for (const arp_param_entry_t& paramEntry : parameterTypes) {
			automatable_param_t automatable{0};
			automatable.idx = idx;
			automatable.internalIdx = -1;
			automatable.category = 0;
			automatable.value = paramEntry.val;
			automatable.label = paramEntry.name;
			automatable.shortLabel = paramEntry.name;
			params.push_back(std::move(automatable));
			idx++;
		}
		params[ARP_PARAM_ENABLED].value = 0;
		params[ARP_PARAM_CLOCK].value = 10/(double)NUM_ARP_STEPSIZE_OPTIONS;
		params[ARP_PARAM_GATE].value = 1/4.0f;
		params[ARP_PARAM_PATTERN].value = 0;
		getAutomation(0)->quantizationSteps = 1;
		getAutomation(1)->quantizationSteps = NUM_ARP_STEPSIZE_OPTIONS-1;
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
		return params[ARP_PARAM_GATE].value;
	}
	void setGateF(float f) {
		params[ARP_PARAM_GATE].value = f;
	}
	float getPatternF() {
		return params[ARP_PARAM_PATTERN].value;
	}
	void setPatternF(float f) {
		params[ARP_PARAM_PATTERN].value = f;
	}
	float getClockF() {
		return params[ARP_PARAM_CLOCK].value;
	}
	void setClockF(float f) {
		params[ARP_PARAM_CLOCK].value = std::max(0.0f, std::min(1.0f, f));
	}
	tick_t getStepSize() {
		int32_t option = (int32_t)std::floor(getClockF()*(NUM_ARP_STEPSIZE_OPTIONS-1));
		assert(option<NUM_ARP_STEPSIZE_OPTIONS);
		int32_t len = tickLength[option];
		assert(len>0);
		return len;
	}
	tick_t getDuration() {
		const int minDuration = getStepSize()>>3;
		const int maxDuration = getStepSize()<<1;
		tick_t len = (tick_t)std::floor(minDuration+getGateF()*(maxDuration-minDuration));
		assert(len>0);
		return len;
	}
	int isChordOutput() {
		int32_t option = (int32_t) std::floor(getPatternF() * (NUM_PATTERNS - 1));
		assert(option<NUM_PATTERNS);
		return option == 0;
	}
	int getStepIdx(int step, int nNotes) {
		int32_t option = (int32_t) std::floor(getPatternF() * (NUM_PATTERNS - 1));
		assert(option<NUM_PATTERNS);
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
		noteEvents.emplace_back(note.pitch, note.velocity, note.start()-start, true, false);
		heldOutputNotes.push_back(note);
		heldOutputAnimationNotes.push_back(note);
		notesSpawnTime.push_back(time);
		assert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
	}


	String getAutomatableName() override {
		return "Arp";
	}
	float convertValFrom(int32_t idx, float f) {
		return f;
	}
	float convertValTo(int32_t idx, float f) {
		return f;
	}
	float getParamValue(int32_t idx) override {
		if (idx >= 0 && idx < (int)params.size()) {
			return convertValFrom(idx, params[idx].value);
		}
		return 0.0f;
	}
	void onEnable() {

	}
	void onDisable() {

	}
	void setParamValue(int32_t idx, float val) override {
		if (idx >= 0 && idx < (int)params.size()) {
			auto& param = params[idx];
			param.value = val;
			if (param.idx == PARAM_ENABLE) {
				bool wasEnable = this->enable;
				this->enable = val > 0;
				if (this->enable != wasEnable) {
					if (this->enable) {
						onEnable();
					} else {
						onDisable();
					}
				}
			} else {
				params[idx].value = convertValTo(idx, val);
			}
		}
	}
	automationlane_snapshot_t toRef() override {
		automationlane_snapshot_t ref;
		ref.type = 2;
		ref.refId = 0;
		return ref;
	}
	void createSnapshot(arp_snapshot& snapshot);
	void loadSnapshot(const arp_snapshot& snapshot);

	void process(std::vector<noteevent_t>& noteEventsIn,
					tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd,
					std::vector<noteevent_t>& noteEventsProcessed);
	int writeOutputNotes(std::vector<noteevent_t>& noteEventsProcessed,
			tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int64_t time);
};
