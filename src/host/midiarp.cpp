#include "midiarp.h"
#include "track.h"
#include "snapshot.h"
#include "color_util.h"

void midiarp::loadSnapshot(const arp_snapshot& snapshot) {
	for (auto p : snapshot.params) {
		if (hasParam(p.idx)) {
			setParamValue(p.idx, p.val);
		}
	}
	for (auto p : snapshot.automatedParams) {
		automation_t* automation = getAutomation(p.targetParam);
		automation->points = p.points;
		automation->active = p.active;
	}
}
void midiarp::createSnapshot(arp_snapshot& snapshot) {
	for (int i = 0; i < getNumParameters(); i++) {
		float val = params[i].value;
		param_snapshot_t snapParam{i, val};
		my_printf("VAL[%d] = %f\n", i, val);
		snapshot.params.push_back(std::move(snapParam));
		automation_t* automation = getAutomation(i);
		automation_view_t automationView;
		if (automation) {
			automationView.targetParam = i;
			automationView.points = automation->points;
			automationView.active = automation->active;
		}
		snapshot.automatedParams.push_back(std::move(automationView));
	}
}


int midiarp::writeOutputNotes(std::vector<noteevent_t>& noteEventsProcessed,
		tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int64_t time) {
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
	assert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
	if (!heldOutputAnimationNotes.empty()) {
		auto i = std::begin(heldOutputAnimationNotes);
		auto j = std::begin(notesSpawnTime);
		while (i != std::end(heldOutputAnimationNotes)) {
			int64_t spawnTime = *j;
			int64_t timeSince = time - spawnTime;
			if (timeSince > 10) {
				i = heldOutputAnimationNotes.erase(i);
				j = notesSpawnTime.erase(j);
			} else {
				i++;
				j++;
			}
			assert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
		}
	}
	assert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
	return nSend;
}
void midiarp::process(std::vector<noteevent_t>& noteEventsIn,
				tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd,
				std::vector<noteevent_t>& noteEventsProcessed) {
#define TIME_STEP (resetTime + step * stepSize)

	int nSend = 0;
	int64_t time = getTimeMillis()/100ULL;
	std::vector<noteevent_t> noteEvents = noteEventsIn;
	std::reverse(noteEvents.begin(), noteEvents.end());
	for (tick_t tick = start; tick < end; tick++) {
		bool enabledBefore = this->enable;
		updateAutomatedParameters(tick);
		if (this->enable != enabledBefore) {
			for (noteevent_t& evt : this->heldInput) {
				String str;
				if (this->enable) {
					str = StringFormat("Note off %s", noteName(evt.pitch));
				} else {
					str = StringFormat("Note on %s", noteName(evt.pitch));
				}
				markers.push_back(marker_t{tick, col(5), str});
				noteevent_t evt2 = evt;
				evt2.isNoteOn = !enable;
				evt2.tickOffsetInBlock = tick - start;
   				noteEventsProcessed.push_back(evt2);
				nSend++;
			}
		}
		tick_t stepSize = getStepSize();
		tick_t noteDuration = getDuration();
		if (stepSize != lastStepSize) {
			int nextStep = (resetTime + step * lastStepSize);
			step = 0;
			while (TIME_STEP < nextStep) {
				step++;
			}
			while (TIME_STEP > nextStep+stepSize) {
				step--;
			}
			markers.push_back(marker_t{tick, col(1)});
			String str = StringFormat("StepSize %d -> %d", lastStepSize, stepSize);
			if (TIME_STEP<start||TIME_STEP>=end) {
				markers.push_back(marker_t{TIME_STEP, col(2), str});
//					my_printf("reset out of range\n", 0);
//					reset(tick);
			} else {
				markers.push_back(marker_t{TIME_STEP, col(3), str});
//					reset(TIME_STEP);
			}
			lastStepSize = stepSize;
		}
//				tick_t step = (start - resetTime + stepSize-1) / stepSize;
		while (!noteEvents.empty()) {
			noteevent_t* evt = &noteEvents.back();
			if (evt->tickOffsetInBlock+start > tick) {
				break;
			}
			if (evt->isNoteOn) {
				if (heldInput.empty()) {
						if (resetMode == ResetMode::NOTE) {
						reset(evt->tickOffsetInBlock+start);
						markers.push_back(marker_t{tick, col(4), "reset first note"});
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
//					// arp received a note off with the correspondending note_on missing -> pass through
//					noteEventsProcessed.push_back(*evt);
//					nSend++;
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
			if (!enable) {

				noteEventsProcessed.push_back(*evt);
				nSend++;
			}
			noteEvents.erase(noteEvents.end());
		}
		if (enable) {
			if (heldInput.size() && TIME_STEP < end) {
				assert(TIME_STEP >= start-stepSize*10000);
				while (TIME_STEP < start) {
					step++;
				}
				tick_t timeStep = TIME_STEP;
					if (timeStep == tick) {
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
					step++;
				}
			}

		} else {
			while (!noteEvents.empty()) {
				noteevent_t* evt = &noteEvents.back();
				if (evt->tickOffsetInBlock+start > tick) {
					break;
				}
				noteEventsProcessed.push_back(*evt);
				noteEvents.erase(noteEvents.end());
			}
		}
	}
	nSend += writeOutputNotes(noteEventsProcessed, start, end, loopStart, loopEnd, time);
	if (nSend)
		sortNoteEvents(noteEventsProcessed);
	numCalls++;
	assert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
#undef TIME_STEP
}
