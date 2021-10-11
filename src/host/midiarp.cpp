#include "midiarp.h"
#include "track.h"
#include "snapshot.h"
#include "color_util.h"
#include "history.h"
#include "mainctrl.h"
//#define PLACE_MARKERS
//#define PLACE_MARKERS_OUTPUT



midiarp::midiarp(track_impl_t* _trImpl) :
		automatable_t(), trackImpl(_trImpl) {
	curRandTimeOffset.resize(NUM_ARP_MAX_POLY_VOICES);
	memset(curRandTimeOffset.data(), 0, curRandTimeOffset.size() * sizeof(float));
	for (int i = 0; i < NUM_ARP_STEPSIZE_OPTIONS; i += 2) {
		tickLength[i + 0] = (TICKS_16TH >> 3) << (i >> 1);
		tickLength[i + 1] = tickLength[i + 0] + (tickLength[i + 0] >> 1);
		dbgassert(tickLength[i + 0] > 0);
	}
	const std::array<arp_param_entry_t, 8> parameterTypes { 
		{ 
		  arp_param_entry_t { PARAM_ENABLE, "Enabled", 0.0f }, 
		  arp_param_entry_t { PARAM_GAIN, "Gain", 1.0f }, 
		  arp_param_entry_t { ARP_PARAM_CLOCK, "Clock", 10.0f / (float) NUM_ARP_STEPSIZE_OPTIONS },
		  arp_param_entry_t { ARP_PARAM_GATE, "Gate", 1 / 4.0f }, 
		  arp_param_entry_t { ARP_PARAM_PATTERN, "Pattern", 0.0f },
		  arp_param_entry_t { ARP_PARAM_RAND_TIME, "Random Time", 0.0f }, 
		  arp_param_entry_t { ARP_PARAM_RAND_MODE, "Random Time Mode", 0.0f }, 
		  arp_param_entry_t { ARP_PARAM_RAND_VEL, "Random Velocity", 0.0f }, 
		} 
	};
	for (const arp_param_entry_t &paramEntry : parameterTypes) {
		automatable_param_t *regparam = registerParam(paramEntry.id);
		regparam->value = paramEntry.val;
		regparam->label = paramEntry.name;
		regparam->shortLabel = paramEntry.name;
	}
	//		setParamValue(ARP_PARAM_ENABLED, 0, FLG_PAR_UPDATE_INIT);
	//		setParamValue(ARP_PARAM_CLOCK, 10/(double)NUM_ARP_STEPSIZE_OPTIONS, FLG_PAR_UPDATE_INIT);
	//		setParamValue(ARP_PARAM_GATE, 1/4.0f, FLG_PAR_UPDATE_INIT);
	//		setParamValue(ARP_PARAM_PATTERN, 0, FLG_PAR_UPDATE_INIT);
	getOrCreateAutomation(PARAM_ENABLE)->quantizationSteps = 1;
	getOrCreateAutomation(ARP_PARAM_CLOCK)->quantizationSteps = NUM_ARP_STEPSIZE_OPTIONS - 1;
	getOrCreateAutomation(ARP_PARAM_RAND_MODE)->quantizationSteps = NUM_RANDOM_TIME_MODES - 1;
}

tick_t midiarp::getStepSize() {
	int32_t option = (int32_t) std::floor(getClockF() * (NUM_ARP_STEPSIZE_OPTIONS - 1));
	dbgassert(option<NUM_ARP_STEPSIZE_OPTIONS);
	int32_t len = tickLength[option];
	dbgassert(len > 0);
	return len;
}

int32_t midiarp::getRandTmMode() {
	int32_t option = (int32_t) std::round(getParamValue(ARP_PARAM_RAND_MODE) * (NUM_RANDOM_TIME_MODES - 1));
	dbgassert(option<NUM_RANDOM_TIME_MODES);
	return option;
}

tick_t midiarp::getDuration() {
	const int minDuration = getStepSize() >> 3;
	const int maxDuration = getStepSize() << 1;
	tick_t len = (tick_t) (std::floor(minDuration + getGateF() * (maxDuration - minDuration)));
	dbgassert(len > 0);
	return len;
}

int midiarp::isChordOutput() {
	int32_t option = (int32_t) std::floor(getPatternF() * (NUM_PATTERNS - 1));
	dbgassert(option<NUM_PATTERNS);
	return option == 0;
}

int midiarp::getStepIdx(int step, int nNotes) {
	int32_t option = (int32_t) std::floor(getPatternF() * (NUM_PATTERNS - 1));
	dbgassert(option<NUM_PATTERNS);
	if (option > 0) {
		option--;
	}
	switch (option) {
	case 0:
		return step % nNotes;
	case 1:
		return nNotes - 1 - (step % nNotes);
	case 2:
		return (step / nNotes) % 2 == 0 ? step % nNotes : (nNotes - 1 - (step % nNotes));
	case 3:
		return (step / nNotes) % 2 == 0 ? (nNotes - 1 - (step % nNotes)) : step % nNotes;
	case 4:
		return step % 2 == 0 ? 0 : (step / 2) % nNotes;
	}
	return 0;
}

void midiarp::loadSnapshot(const arp_snapshot& snapshot) {
	for (const auto& param : snapshot.params) {
		if (!getParam(param.idx)) {
			//version mismatch
			return;
		}
	}
	for (const auto& param : snapshot.params) {
		setParamValue(param.idx, param.val, FLG_PAR_UPDATE_INIT);
	}
	loadAutomation(snapshot.automatedParams, this);
}
void midiarp::postSetParameter(int32_t idx, float preVal, float val, int flags) {
	if (flags != 2) {
		return;
	}
	dbgassert(this->trackImpl->getTrack());
	track_t* track = this->trackImpl->getTrack();
	automationlane_snapshot_t ref = toRef();
	parameter_ref_t p = {track->projectIdx,  ref.type, 0, idx};
	DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
}
void midiarp::createSnapshot(arp_snapshot& snapshot) {
	snapshot.params.reserve(getNumParameters());
	visitParams([&snapshot](auto& mapEntry) {
		automatable_param_t& param = mapEntry.second;
		snapshot.params.push_back(param_snapshot_t{param.idx, param.value});
	});
	storeAutomation(snapshot.automatedParams, this);
}

template<typename T>
void cleanupMarkers(T& markers, tick_t tickTmNow) {
	if (!markers.empty()) {
		int markersSize = markers.size();
		auto i = std::begin(markers);
		while (i != std::end(markers)) {
			int64_t spawnTick = (*i).time;
			int64_t timeSince = tickTmNow - spawnTick;
			if (timeSince > TICKS_QUARTER*16 || markersSize > 100) {
				i = markers.erase(i);
				markersSize--;
			} else {
				i++;
			}
		}
	}
}

void midiarp::onStartPlayback() {
	markers.clear();
	markers2.clear();
}
void midiarp::allNotesOff(std::vector<noteevent_t>& noteEvents) {
	stepGenerated = -1;
	heldInput.clear();
	int nSend = 0;
	if (!heldOutputNotes.empty()) {
		auto itHeldOutNotes = std::begin(heldOutputNotes);
		while (itHeldOutNotes != std::end(heldOutputNotes)) {
			note_t& note = *itHeldOutNotes;
			//force off
			noteEvents.emplace_back(note.pitch, note.velocity, 0, note.start(), false, false);
			itHeldOutNotes = heldOutputNotes.erase(itHeldOutNotes);
		}
	}
	dbgassert(heldOutputNotes.empty());
	heldOutputAnimationNotes.clear();
	notesSpawnTime.clear();
}

int32_t midiarp::getRandVelocity() {
	float fRandVel = pow(getRandVelocityF(), 2.0f);
	tick_t len = (tick_t) (std::round(fRandVel * (127)));
	return len;
}

tick_t midiarp::getRandTime() {
	float fRandTm = pow(getRandTimeF(), 2.0f);
	const int minDuration = 0; //math::max(0, getStepSize() >> 4);
	const int maxDuration = math::max(0, getStepSize() >> 1);
	tick_t len = (tick_t) (std::floor(minDuration + fRandTm * (maxDuration - minDuration)));
	return len;
}

int midiarp::writeOutputNotes(std::vector<noteevent_t>& noteEventsProcessed,
		tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int64_t time) {

	dbgassert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
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
			dbgassert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
		}
	}
	dbgassert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
	cleanupMarkers(markers, end);
	cleanupMarkers(markers2, end);
	return 0;
}

void midiarp::addNote(std::vector<noteevent_t>& noteEvents, tick_t start, note_t& note, int64_t time) {
	//		int32_t noteVelocity = math::clamp<int32_t>(this->getGainF() * note.velocity, 0, 127);
	//find overlapping held notes;
	note_t noteInput = note;
	int retVal = cutNoteOutOfList(heldOutputNotes, note, true);
	if (retVal == -1) {
 		log_printf("exact duplicate in list!\n", 0);
		//dbgassert(0);
	} else if (retVal != 0) {
		log_printf("intersecting. ret val %d\n", retVal);
	}

	noteEvents.emplace_back(note.pitch, note.velocity, note.start() - start, note.start(), true, false);
	heldOutputNotes.push_back(note);
	notesSpawnTime.push_back(time);
	cutNoteOutOfList(heldOutputAnimationNotes, noteInput, true);
	heldOutputAnimationNotes.push_back(note);
	dbgassert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
}
void midiarp::initRandomDelays(uint64_t seed, int32_t step, int32_t stepSize, int32_t startFrame, int32_t endFrame, bool reset)
{
	struct arp_generated_pattern_step {
		tick_t time;
		int32_t index;
	};
	std::vector<arp_generated_pattern_step> arpSteps;
	uint64_t stepSeed_u64 = seed ^ (uint64_t)resetTime ^ (uint64_t)math::max<int32_t>(0, step - 1) * stepSize;
//		uint64_t seedu64 = (lSeed^evt.globalTick)^((evt.pitch<<12)|(evt.velocity));
    arpRand.rng_seed(stepSeed_u64);
    int32_t randMode = getRandTmMode();
	bool noRandomOnReset = true;
    int32_t rndTime = noRandomOnReset && step == 0 ? 0 : this->getRandTime();
	
    for (int i = 0; i < NUM_ARP_MAX_POLY_VOICES; i++) {
		uint32_t rnd_u32 = arpRand.rng_rand(rndTime);
		if (noRandomOnReset && step == 0) {
			curRandTimeOffset[i] = 0;
		} else if (randMode == 0) {
			curRandTimeOffset[i] = rnd_u32;
		} else {
			curRandTimeOffset[i] = -rndTime + arpRand.rng_rand(rndTime*2);
		}
		tick_t rndStepTime = resetTime + step * stepSize + curRandTimeOffset[i];
		arpSteps.push_back(arp_generated_pattern_step{rndStepTime, i});
	}
	std::sort(arpSteps.begin(), arpSteps.end(), [](const arp_generated_pattern_step& a, const arp_generated_pattern_step& b) {
		if (a.time == b.time) {
			return a.index < b.index;
		}
		return a.time < b.time;
	});
#ifdef PLACE_MARKERS
    markers2.clear();
    for (int i = 0; i < arpSteps.size()&&i<6 ; i++) {
    	auto& procTmPt = arpSteps[i];
		
		String str = StringFormat("@%d rndStepTime[%d]", procTmPt.time, procTmPt.index);
		markers2.push_back(marker_t{procTmPt.time, col(7), str, (float)(i)});
    }
#endif
    processTimePoints.clear();
    processNotesSpawn.clear();
    for (int i = 0; i < arpSteps.size(); i++) {
    	auto& procTmPt = arpSteps[i];
		processTimePoints.push_back(procTmPt.time);
		processNotesSpawn.push_back(procTmPt.index);
	}


}
int midiarp::endOutputNotes(tick_t tick, tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, std::vector<noteevent_t>& noteEventsProcessed) {
	int nSend = 0;
	if (!heldOutputNotes.empty()) {
		auto i = std::begin(heldOutputNotes);
		while (i != std::end(heldOutputNotes)) {
			note_t& note = *i;
			if (note.end() <= tick || note.end() < start) {
				auto tickOffsetInBlockEnd = tick-start-1;
				if (tickOffsetInBlockEnd < 0) {
					dbgassert(note.end() <= start);
					log_printf("ending output note with end() < blockStart\n", 0);
					tickOffsetInBlockEnd = 0;
				}
#ifdef PLACE_MARKERS
				markers.push_back(marker_t{tick, col(5), StringFormat("%d end %s @%d %d %d", tick, noteName(note.pitch), note.end(), note.len, tickOffsetInBlockEnd), (float)(tickMarkers++)});
#endif
				noteEventsProcessed.emplace_back(note.pitch, note.velocity, tickOffsetInBlockEnd, note.start(), false, note.end() == loopEnd);
				nSend++;
				i = heldOutputNotes.erase(i);
			}
			else i++;
		}
	}
	return nSend;
}
void midiarp::process(std::vector<noteevent_t>& noteEventsIn,
				tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd,
				std::vector<noteevent_t>& noteEventsProcessed, tick_t ticksPerBlock) {
	this->tickMarkers = 0;
	//#ifdef PLACE_MARKERS
	//						markers.push_back(marker_t{start, col(7), StringFormat("process %d evts", noteEventsIn.size()), (float)(tickMarkers++)});
	//#endif
	//#ifdef PLACE_MARKERS
	//						markers.push_back(marker_t{end, col(7), "block end", (float)(tickMarkers++)});
	//#endif

	int64_t time = getTimeMillis()/100ULL;
	std::vector<noteevent_t> noteEvents = noteEventsIn;
	sortNoteEvents(noteEvents);
	std::reverse(noteEvents.begin(), noteEvents.end());

	numCalls++;
	if (!this->enable) {
		const automation_t* automatEnable = getRegisteredConstAutomation(PARAM_ENABLE);
		if (!automatEnable || !automatEnable->active) {
			noteEventsProcessed = noteEventsIn;
			return;
		}
	}
	if (noteEventsIn.empty()
			&& this->heldInput.empty()
			&& this->heldOutput.empty()
			&& this->heldOutputNotes.empty()
			&& this->heldInputAnimationNotes.empty()
			&& this->heldOutputAnimationNotes.empty()) {
		noteEventsProcessed = noteEventsIn;
		return;
	}
	int nSend = 0;
    bool onceProcMsg = false;
	for (tick_t t = start; t < end; t++) {
		const tick_t tick = t;
		bool enabledBefore = this->enable;
		updateAutomatedParameters(tick);
		tick_t stepSize = getStepSize();
		tick_t noteDuration = getDuration();
		if (this->enable != enabledBefore) {
			for (noteevent_t& evt : this->heldInput) {
#ifdef PLACE_MARKERS
				String str;
				if (this->enable) {
					str = StringFormat("Note off %s", noteName(evt.pitch));
				} else {
					str = StringFormat("Note on %s", noteName(evt.pitch));
				}
				markers.push_back(marker_t{tick, col(5), str, (float)(tickMarkers++)});
#endif
				noteevent_t evt2 = evt;
				evt2.isNoteOn = !enable;
				evt2.tickOffsetInBlock = tick - start;
   				noteEventsProcessed.push_back(evt2);
				nSend++;
			}
		}
		endOutputNotes(tick, start, end, loopStart, loopEnd, noteEventsProcessed);
//#define TIME_STEP (resetTime + step * stepSize)
//			int newStep = step;
//			while (tick < resetTime + newStep * stepSize) {
//				newStep++;
//			}
//			while (tick > resetTime + newStep * stepSize+stepSize) {
//				newStep--;
//			}
//			step = newStep;
		//if (resetTime && heldInput.size() && stepSize != lastStepSize) {
//#ifdef PLACE_MARKERS
//			markers.push_back(marker_t{tick, col(1), "", (float)(tickMarkers++)});
//			String str = StringFormat("StepSize %d -> %d", lastStepSize, stepSize);
//			if (TIME_STEP<start||TIME_STEP>=end) {
//				markers.push_back(marker_t{TIME_STEP, col(2), str, (float)(tickMarkers++)});
////					my_printf("reset out of range\n", 0);
////					reset(tick);
//			} else {
//				markers.push_back(marker_t{TIME_STEP, col(3), str, (float)(tickMarkers++)});
////					reset(TIME_STEP);
//			}
//#endif
            //initRandomDelays(lSeed, step, stepSize, start, end, false);
			//lastStepSize = stepSize;
        //}
        //				tick_t step = (start - resetTime + stepSize-1) / stepSize;

		for (noteevent_t& noteevt : noteEvents) {
			if (noteevt.tickOffsetInBlock+start != tick) {
				continue;
			}
			if (!noteevt.isNoteOn) {
				auto it = std::find_if(heldInput.begin(), heldInput.end(), [&noteevt](const noteevent_t evt2) {
					return noteevt.pitch == evt2.pitch;
				});
				if (it == heldInput.end()) {
					// arp received a note off with the corresponding note_on missing -> pass through
					// when playback starts in middle of note
				} else {
#ifdef PLACE_MARKERS
					markers.push_back(marker_t{tick, col(5), StringFormat("END IN %s", noteName((*it).pitch)), (float)(tickMarkers++)});
#endif
					heldInput.erase(it);
					auto it2 = std::find_if(heldInputAnimationNotes.begin(), heldInputAnimationNotes.end(), [&noteevt](const note_t evt2) {
						return noteevt.pitch == evt2.pitch;
					});
					if (it2 != heldInputAnimationNotes.end()) {
						heldInputAnimationNotes.erase(it2);
					}
				}
			}

		}
		const bool isReset = heldInput.empty();
		for (noteevent_t& noteevt : noteEvents) {
			noteevent_t* evt = &noteevt;
			if (evt->tickOffsetInBlock+start != tick) {
				continue;
			}
			if (evt->isNoteOn) {
				auto it = std::find_if(heldInput.begin(), heldInput.end(), [evt](const noteevent_t evt2) {
					return evt->pitch == evt2.pitch;
				});
				if (it == heldInput.end()) {
					if (heldInput.empty()) {
						//if (resetMode == ResetMode::NOTE) {
							reset(evt->tickOffsetInBlock+start);
	#ifdef PLACE_MARKERS
							markers.push_back(marker_t{tick, col(4), "reset first note", (float)(tickMarkers++)});
	#endif
							initRandomDelays(lSeed, step, stepSize, start, end, true);
						//}
					} else if (processTimePoints.size() < heldInput.size()+1) {
						//initRandomDelays(lSeed, step, stepSize, start, end, false);
					}
					note_t note;
					note.time = evt->tickOffsetInBlock+start;
					note.pitch = evt->pitch;
					note.velocity = evt->velocity;
					note.len = TICKS_QUARTER*2;
 					noteevent_t& nevt = *evt;
					heldInput.push_back(nevt);
#ifdef PLACE_MARKERS
					markers.push_back(marker_t{note.time, col(5), StringFormat("Note Start IN %s", noteName(note.pitch)), (float)(tickMarkers++)});
#endif
		/*			std::sort(heldInput.begin(), heldInput.end(), [](const noteevent_t evt1, const noteevent_t evt2) {
						return evt1.pitch < evt2.pitch;
					});*/
					heldInputAnimationNotes.push_back(note);
					maxNoteChordCount = math::clamp<int32_t>(heldInput.size()-1, 0, curRandTimeOffset.size()-1);
					if (!isReset) {
						if (note.time < (resetTime+step*stepSize)+noteDuration-1) {
#ifdef PLACE_MARKERS
							markers.push_back(marker_t{note.time, col(3), StringFormat("late note IN %s", noteName(note.pitch)), (float)(tickMarkers++)});
#endif
							//auto it = std::find_if(heldInput.begin(), heldInput.end(), [&nevt](const noteevent_t evt2) {
							//	return nevt.pitch == evt2.pitch && nevt.tickOffsetInBlock == evt2.tickOffsetInBlock;
							//});
							//int32_t newNoteIdx = it - heldInput.begin();
							int32_t newNoteIdx = maxNoteChordCount;
							for (int idxVoice = 0; idxVoice < NUM_ARP_MAX_POLY_VOICES; idxVoice++) {
								if (processNotesSpawn[idxVoice] == newNoteIdx) {
  									curRandTimeOffset[idxVoice]=note.time - resetTime;
									processTimePoints[idxVoice]=note.time;
#ifdef PLACE_MARKERS
    markers2.clear();
    for (int i2 = 0; i2 < processNotesSpawn.size()&&i2<12; i2++) {
		String str = StringFormat("@%d latenotefix[%d]", processTimePoints[i2], processNotesSpawn[i2]);
		markers2.push_back(marker_t{processTimePoints[i2], col(6), str, (float)(i2)});
    }
#endif
									break;
								}
							}
						}
					}
				} else {
	#ifdef PLACE_MARKERS
					markers.push_back(marker_t{evt->tickOffsetInBlock+start, col(1), StringFormat("Note Duplicate Held IN %s", noteName(evt->pitch)), (float)(tickMarkers++)});
	#endif
				}

			}
			if (!enable) {

				noteEventsProcessed.push_back(*evt);
				nSend++;
			}
		}
		if (enable) {
			if (tick >= (resetTime+step*stepSize))
			{
				const auto tmTickCur = tick;
				if (heldInput.size()) {
					if (tick == resetTime+(step+1)*stepSize) {
						step++;
					}
					if (tick >= (resetTime+step*stepSize) + (stepSize/2)) {
						if (stepGenerated != step+1) {
							if(tick >= resetTime+(step+1)*stepSize) {
								log_printf("missed a step, did stepsize change?\n", 0);
							}
							bool stepCompleted = std::all_of(std::begin(processTimePoints), std::end(processTimePoints), [tmTickCur](tick_t t){
								return t <= tmTickCur;
							});
							if (stepCompleted) {
								initRandomDelays(lSeed, step+1, stepSize, start, end, false);
								bool stepCompleted2 = std::all_of(std::begin(processTimePoints), std::end(processTimePoints), [tmTickCur](tick_t t){
									return t <= tmTickCur;
								});
								if(stepCompleted2) {
									log_printf("no unprocessed steps (%d) generated at step %d range %d %d\n", processTimePoints.size(), step+1, start, end);
								}
								stepGenerated = step+1;
							}
						} else {
//							log_printf("skip step, already generated\n", 0);
						}
					}
				}
			}

			int currentpolyphonecount = heldInput.size();
			for (int prIdx = 0; prIdx < processTimePoints.size() && (prIdx == 0 || isChordOutput()); prIdx++) {
                tick_t timeStep = processTimePoints[prIdx];
                if (timeStep < start) {
                    //dbgassert(0);
                    continue;
                }
                if (timeStep >= end) {
                    continue;
                }
                if (timeStep != tick)
                    continue;
//				const tick_t tickConst = tick;
//                tick_t offsetFromRegTick = tick - (resetTime+step*stepSize);
				dbgassert(processNotesSpawn.size() > prIdx && prIdx >= 0);
				dbgassert(processNotesSpawn[prIdx] >= 0);
				dbgassert(processNotesSpawn[prIdx] >= 0);
//				bool voiceLimitHit = heldOutputNotes.size() >= currentpolyphonecount+heldInput.size();
				int noteIdx = processNotesSpawn[prIdx];
				int spawnNotes = 1<<(noteIdx);
#ifdef PLACE_MARKERS
				markers.push_back(marker_t{timeStep, col(2347), StringFormat("stp prIdx %d noteIdx %d heldnotes: %d", prIdx, noteIdx, heldOutput.size()), (float)(tickMarkers++)});
#endif
//				if (heldOutputNotes.size() >= currentpolyphonecount) {
//					break;
//				}
				#if 0
                //log_printf("process processTimePoints[%d] %d, offsetFromRegTick %d\n", prIdx, processTimePoints[prIdx], offsetFromRegTick);
                int32_t maxNote = math::min<int32_t>(maxNoteChordCount, 
					isChordOutput() ? heldInput.size() : math::min <int32_t>(heldInput.size(), 1));

                int indexProcessed = cur;
                for (int i = 0; i < processNotesSpawn.size(); i++) {
                    if (offsetFromRegTick == curRandTimeOffset[i]) {
                        //log_printf("timeStep[%d] %d offsetFromRegTick %d\n", prIdx, tick, offsetFromRegTick);
                    	indexProcessed = i;
                    	break;
                    }
                }
                dbgassert(offsetFromRegTick == 0 || indexProcessed != -1);
    //        	log_printf("handle timeStep %d\n", timeStep);
    //        	log_printf("processTimePoints size %d\n", processTimePoints.size());
    //        	log_printf("maxNote %d\n", maxNote);
				int spawnNotes = 0;
                for (int i = 0; i < maxNote; i++) {
//	            	log_printf("curRandTimeOffset[%d] %d\n", i, curRandTimeOffset[i]);
                    if (offsetFromRegTick == curRandTimeOffset[i]) {
                    	//log_printf("playnote %d\n", maxNote);
						spawnNotes |= (1<<i);
					}
				}
				#endif
#ifdef PLACE_MARKERS2
                String str;
                str = StringFormat("pt  0x%02X", spawnNotes);
                markers.push_back(marker_t{timeStep, col(6), str, (float)(tickMarkers++)});
                str = StringFormat("prIdx %d", prIdx);
                markers.push_back(marker_t{timeStep, col(6), str, (float)(tickMarkers++)});
                str = StringFormat("step %d", step);
                markers.push_back(marker_t{timeStep, col(6), str, (float)(tickMarkers++)});
                str = StringFormat("timeStep %d", timeStep);
                markers.push_back(marker_t{timeStep, col(6), str, (float)(tickMarkers++)});
                str = StringFormat("spawnNotes %d", spawnNotes);
                markers.push_back(marker_t{timeStep, col(6), str, (float)(tickMarkers++)});
                str = StringFormat("heldInput %d", heldInput.size());
                markers.push_back(marker_t{timeStep, col(6), str, (float)(tickMarkers++)});
#endif
				if (spawnNotes) {
					note_t note;
					note.time = tick;
					dbgassert(note.time >= start && note.time < end);
					note.len = noteDuration;
					if (isChordOutput()) {
						for (int idx = 0; idx < (int)heldInput.size(); idx++) {
							const noteevent_t& evt = heldInput[idx];
							if (spawnNotes & (1<<idx))  {
								note_t noteChord = note;
								noteChord.pitch = evt.pitch;
								noteChord.velocity = evt.velocity;
								int32_t rndVelIntensity = this->getRandVelocity();
								if (rndVelIntensity) {
									uint64_t seedu64 = (lSeed^evt.globalTick)^((evt.pitch<<12)|(evt.velocity));
									arpRand.rng_seed(seedu64);
									tick_t randVel = -rndVelIntensity + arpRand.rng_rand(rndVelIntensity*2);
									noteChord.velocity = math::clamp(noteChord.velocity+randVel, 0, 127);
								}

#ifdef PLACE_MARKERS_OUTPUT
						String str = StringFormat("noteChord %d %d %s", noteChord.time, noteChord.len, noteName(noteChord.pitch));
						markers.push_back(marker_t{tick, col(2), str, (float)(tickMarkers++)});
#endif
								addNote(noteEventsProcessed, start, noteChord, time);

								nSend++;
							}
						}
					} else if (heldInput.size()) {
						int idx = getStepIdx(step, heldInput.size());
						const noteevent_t& evt = heldInput[idx];
						note.pitch = evt.pitch;
						note.velocity = evt.velocity;
						int32_t rndVelIntensity = this->getRandVelocity();
						if (rndVelIntensity) {
							uint64_t seedu64 = (lSeed^evt.globalTick)^((evt.pitch<<12)|(evt.velocity));
							arpRand.rng_seed(seedu64);
							tick_t randVel = arpRand.rng_rand(rndVelIntensity);
							note.velocity = math::clamp(note.velocity+randVel, 0, 127);
						}
						addNote(noteEventsProcessed, start, note, time);
#ifdef PLACE_MARKERS_OUTPUT
						String str = StringFormat("noteSingle %d %d %s", note.time, note.len, noteName(note.pitch));
						markers.push_back(marker_t{tick, col(6), str, (float)(tickMarkers++)});
#endif
						nSend++;
					}
				}
            }
			
		} else if (false) {
			while (!noteEvents.empty()) {
				noteevent_t* evt = &noteEvents.back();
				if (evt->tickOffsetInBlock+start > tick) {
					break;
				}
				noteEventsProcessed.push_back(*evt);
				noteEvents.pop_back();
			}
		}
	}
	endOutputNotes(end-1, start, end, loopStart, loopEnd, noteEventsProcessed);
	nSend += writeOutputNotes(noteEventsProcessed, start, end, loopStart, loopEnd, time);
	if (nSend)
		sortNoteEvents(noteEventsProcessed);
	dbgassert(notesSpawnTime.size() == heldOutputAnimationNotes.size());
#undef TIME_STEP
}
