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

class midiarp {
public:
	enum ResetMode
		: int {NOTE, BEAT
	};
private:
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
	tick_t resetTime = 0;
	int noteIdx = 0;
	float fStepSize = 0;
	float fGate = 0;
	float fPattern = 0;
	std::array<tick_t, 16*3> tickLength;
public:
	bool enable = false;
	midiarp() {
		for (int i = 0; i < NUM_ARP_STEPSIZE_OPTIONS; i += 2) {
			tickLength[i + 0] = (TICKS_16TH >> 3) << (i >> 1);
			tickLength[i + 1] = tickLength[i + 0] + (tickLength[i + 0] >> 1);
			assert(tickLength[i + 0] > 0);
		}
		fStepSize = 10/(double)NUM_ARP_STEPSIZE_OPTIONS;
		fGate = 1/4.0f;
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
		return fGate;
	}
	void setGateF(float f) {
		fGate = f;
	}
	float getPatternF() {
		return fPattern;
	}
	void setPatternF(float f) {
		fPattern = f;
	}
	float getClockF() {
		return fStepSize;
	}
	void setClockF(float f) {
		fStepSize = std::max(0.0f, std::min(1.0f, f));
	}
	tick_t getStepSize() {
		int32_t option = (int32_t)std::floor(fStepSize*NUM_ARP_STEPSIZE_OPTIONS);
		return tickLength[option];
	}
	tick_t getDuration() {
		const int minDuration = getStepSize()>>3;
		const int maxDuration = getStepSize()<<1;
		return (int32_t)std::floor(minDuration+fGate*(maxDuration-minDuration));
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
								note_t note;
								note.time = evt->tickOffsetInBlock+start;
								assert(note.time >= start && note.time < end);
								note.pitch = 12*2;
								note.velocity = evt->velocity;
								note.len = TICKS_QUARTER;
								heldOutputAnimationNotes.push_back(note);
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
					int idx = step % (int) heldInput.size();
					noteevent_t evt = heldInput[idx];
					note_t note;
					note.time = timeStep;
					assert(note.time >= start && note.time < end);
					note.pitch = evt.pitch;
					note.velocity = evt.velocity;
					note.len = noteDuration;
					noteEventsProcessed.emplace_back(note.pitch, note.velocity, note.start()-start, true, false);
					heldOutputNotes.push_back(note);
					heldOutputAnimationNotes.push_back(note);
					notesSpawnTime.push_back(time);
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
};
