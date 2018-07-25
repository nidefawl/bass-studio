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
using std::min;
using std::max;

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
	std::vector<uint64_t> notesSpawnTime;
private:
#define NUM_ARP_STEPSIZE_OPTIONS 16
#define NUM_PATTERNS 6
	tick_t resetTime = 0;
	int noteIdx = 0;
	std::array<tick_t, 16*3> tickLength;
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
		my_printf("reset\n", 0);
	}
	void allNotesOff() {
		heldInput.clear();
		heldOutput.clear();
		heldOutputNotes.clear();
		heldInputAnimationNotes.clear();
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
		int32_t option = (int32_t)std::floor(getClockF()*NUM_ARP_STEPSIZE_OPTIONS);
		return tickLength[option];
	}
	tick_t getDuration() {
		const int minDuration = getStepSize()>>3;
		const int maxDuration = getStepSize()<<1;
		return (int32_t)std::floor(minDuration+getGateF()*(maxDuration-minDuration));
	}

	int writeOutputNotes(std::vector<noteevent_t>& noteEventsProcessed,
			tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd) {
		int nSend = 0;
		if (!heldOutputNotes.empty()) {
			auto i = std::begin(heldOutputNotes);
			while (i != std::end(heldOutputNotes)) {
				note_t& note = *i;
				if (note.end() > start && note.end() <= end) {
					noteEventsProcessed.emplace_back(note.pitch, note.velocity, note.end()-start-1, false, note.end() == loopEnd);
					nSend++;
					i = heldOutputNotes.erase(i);
				}
				else i++;
			}
		}
		if (!heldOutputAnimationNotes.empty()) {
			int64_t time = getTimeMillis();
			auto i = std::begin(heldOutputAnimationNotes);
			auto j = std::begin(notesSpawnTime);
			while (i != std::end(heldOutputAnimationNotes)) {
				if (time - *j > 1000) {
//					note_t& note = *i;
//					auto it = std::find_if(heldOutputNotes.begin(), heldOutputNotes.end(), [&note](const noteevent_t evt2) {
//						return note.pitch == evt2.pitch && note.time == evt2.tickOffsetInBlock;
//					});
					i = heldOutputAnimationNotes.erase(i);
					j = notesSpawnTime.erase(j);
				} else {
					i++;
					j++;
				}
			}
		}
		return nSend;
	}
	int isChordOutput() {
		int32_t option = (int32_t)std::floor(getPatternF()*NUM_PATTERNS);
		return option == 0;
	}
	int getStepIdx(int step, int nNotes) {
		int32_t option = (int32_t)std::floor(getPatternF()*NUM_PATTERNS);
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
	void process(std::vector<noteevent_t>& noteEventsIn,
					tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd,
					std::vector<noteevent_t>& noteEventsProcessed) {
		int nSend = 0;
		if (enable) {
			int64_t time = getTimeMillis();
			std::vector<noteevent_t> noteEvents = noteEventsIn;
			std::reverse(noteEvents.begin(), noteEvents.end());
			tick_t stepSize = getStepSize();
			tick_t noteDuration = getDuration();
			tick_t step = (start - resetTime + stepSize-1) / stepSize;
			bool cond = resetTime+step*stepSize>=start;
			if (!cond)
				assert(0);
			while (true) {
				noteevent_t* evt = noteEvents.empty() ? nullptr : &noteEvents.back();
				tick_t timeStep = resetTime+step*stepSize;
				if (evt && start+evt->tickOffsetInBlock <= timeStep) {
					if (evt->isNoteOn) {
						if (heldInput.empty()) {
							if (resetMode == ResetMode::NOTE) {
								reset(evt->tickOffsetInBlock+start);
								step = 0;
								notesSpawnTime.push_back(time);
							}
						}
						note_t note;
						note.time = evt->tickOffsetInBlock+start;
						note.pitch = evt->pitch;
						note.velocity = evt->velocity;
						note.len = TICKS_QUARTER*2;
						heldInputAnimationNotes.push_back(note);
						noteevent_t& nevt = *evt;
						heldInput.push_back(nevt);
						std::sort(heldInput.begin(), heldInput.end(), [](const noteevent_t evt1, const noteevent_t evt2) {
							return evt1.pitch < evt2.pitch;
						});
					} else {
						auto it = std::find_if(heldInput.begin(), heldInput.end(), [evt](const noteevent_t evt2) {
							return evt->pitch == evt2.pitch;
						});
						if (it == heldInput.end()) {
							// arp received a note off with the correspondending note_on missing -> pass through
							noteEventsProcessed.push_back(*evt);
							nSend++;
						} else {
							heldInput.erase(it);
							auto it2 = std::find_if(heldInputAnimationNotes.begin(), heldInputAnimationNotes.end(), [evt](const note_t evt2) {
								return evt->pitch == evt2.pitch;
							});
							if (it2 != heldInputAnimationNotes.end()) {
								heldInputAnimationNotes.erase(it2);
							}
						}
					}
					noteEvents.erase(noteEvents.end());
					continue;
				}
				if (timeStep >= end) {
					break;
				}
				if (heldInput.size()) {
					note_t note;
					note.time = timeStep;
					assert(note.time >= start && note.time < end);
					note.len = noteDuration;
					if (isChordOutput()) {
						for (int idx = 0; idx < (int)heldInput.size(); idx++) {
							noteevent_t evt = heldInput[idx];
							note_t noteChord = note;
							noteChord.pitch = evt.pitch;
							noteChord.velocity = evt.velocity;
							addNote(noteEventsProcessed, start, noteChord, time);
						}
					} else {
						int idx = getStepIdx(step, heldInput.size());
						noteevent_t evt = heldInput[idx];
						note.pitch = evt.pitch;
						note.velocity = evt.velocity;
						addNote(noteEventsProcessed, start, note, time);
					}


					nSend++;
				}
				step++;
			}
		} else {
			std::copy(noteEventsIn.begin(), noteEventsIn.end(), back_inserter(noteEventsProcessed));
		}
		nSend += writeOutputNotes(noteEventsProcessed, start, end, loopStart, loopEnd);
		if (nSend)
			sortNoteEvents(noteEventsProcessed);
	}
	void addNote(std::vector<noteevent_t>& noteEvents, tick_t start, note_t& note, int64_t time) {
		noteEvents.emplace_back(note.pitch, note.velocity, note.start()-start, true, false);
		heldOutputNotes.push_back(note);
		heldOutputAnimationNotes.push_back(note);
		notesSpawnTime.push_back(time);
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
//	void createSnapshot(track_params_snapshot_t& snapshot);
//	void loadSnapshot(const track_params_snapshot_t& snapshot);
};
