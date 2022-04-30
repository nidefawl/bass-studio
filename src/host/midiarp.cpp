#include "midiarp.h"
#include "logging.h"
#include "platform.h"
#include "track.h"
#include "snapshot.h"
#include "history.h"
#include "mainctrl.h"


//#define PLACE_MARKERS
//#define PLACE_MARKERS_OUTPUT

namespace {
    constexpr bool logProcessedNotes = false;
}

static std::array<tick_t, 16 * 3> getStaticReadOnlyTickLengthArray() noexcept {
    std::array<tick_t, 16 * 3> tickLength{};
    for (int i = 0; i < NUM_ARP_STEPSIZE_OPTIONS; i += 2) {
        tickLength[i + 0] = (TICKS_16TH >> 3) << (i >> 1);
        tickLength[i + 1] = tickLength[i + 0] + (tickLength[i + 0] >> 1);
        dbgassert(tickLength[i + 0] > 0);
    }
    return tickLength;
}

const std::array<tick_t, 16 * 3> midiarp::tickLength = getStaticReadOnlyTickLengthArray();

midiarp::midiarp(track_impl_t* _trImpl) : automatable_t(), trackImpl(_trImpl) {
    curRandTimeOffset.resize(NUM_ARP_MAX_POLY_VOICES);
    memset(curRandTimeOffset.data(), 0, curRandTimeOffset.size() * sizeof(float));
    const std::array<arp_param_entry_t, 8> parameterTypes{ {
        arp_param_entry_t{ PARAM_ENABLE, "Enabled", "", 0.0f },
        arp_param_entry_t{ PARAM_GAIN, "Gain", "dB", 1.0f },
        arp_param_entry_t{ ARP_PARAM_CLOCK, "Clock", "Ticks", 10.0f / (float) NUM_ARP_STEPSIZE_OPTIONS },
        arp_param_entry_t{ ARP_PARAM_GATE, "Gate", "Ticks", 1 / 4.0f },
        arp_param_entry_t{ ARP_PARAM_PATTERN, "Pattern", "", 0.0f },
        arp_param_entry_t{ ARP_PARAM_RAND_TIME, "Random Time", "Ticks", 0.0f },
        arp_param_entry_t{ ARP_PARAM_RAND_MODE, "Random Time Mode", "", 0.0f },
        arp_param_entry_t{ ARP_PARAM_RAND_VEL, "Random Velocity", "", 0.0f },
    } };
    for (const arp_param_entry_t& paramEntry : parameterTypes) {
        automatable_param_t* regparam = registerParam(paramEntry.id);

        regparam->value = paramEntry.val;
        regparam->name  = paramEntry.name;
        regparam->unit  = paramEntry.unit;
    }

    getOrCreateAutomation(PARAM_ENABLE)->quantizationSteps = 1;
    if (syncClock) {
        getOrCreateAutomation(ARP_PARAM_CLOCK)->quantizationSteps = NUM_ARP_STEPSIZE_OPTIONS - 1;
    }
    getOrCreateAutomation(ARP_PARAM_RAND_MODE)->quantizationSteps = NUM_RANDOM_TIME_MODES - 1;
    getOrCreateAutomation(ARP_PARAM_PATTERN)->quantizationSteps   = NUM_PATTERNS - 1;
}

tick_t midiarp::getStepSize() {

    if (syncClock) {
        int32_t option = (int32_t) std::floor(getParamValue(ARP_PARAM_CLOCK) * (NUM_ARP_STEPSIZE_OPTIONS - 1));
        dbgassert(option < NUM_ARP_STEPSIZE_OPTIONS);
        int32_t len = tickLength[option];
        return len;
    }
    float scMin = 1.0f / 8.0f;
    float scMax = 4.0f * 4.0f * 4.0f;
    float expo  = math::calcExponentForScale(0.5f, 4.0f, scMin, scMax);
    //TODO: hardcode or constexpr this exponent

    float valueMapped = TICKS_16TH * math::calcMappedValueForScale(getParamValue(ARP_PARAM_CLOCK), expo, scMin, scMax);
    dbgassert(valueMapped > 0);
    return math::floorfS32(valueMapped);
}

int32_t midiarp::getRandTmMode() {
    auto option = (int32_t) std::round(getParamValue(ARP_PARAM_RAND_MODE) * (NUM_RANDOM_TIME_MODES - 1));
    dbgassert(option < NUM_RANDOM_TIME_MODES);
    return option;
}

tick_t midiarp::getDuration() {
    //const int minDuration = getStepSize() >> 3;
    //const int maxDuration = getStepSize() << 1;
    //tick_t len = (tick_t) (std::floor(minDuration + getGateF() * (maxDuration - minDuration)));

    // range is 1/8 to 2 times the StepSize
    // I want f = 0.5 to map to 1.0
    float scMin = 1.0f / 8.0f;
    float scMax = 2.0f;
    float expo  = 1.1f;//math::calcExponentForScale(0.5f, 1.0f, scMin, scMax);
                       //TODO: hardcode or constexpr this exponent

    float valueMapped = pow(getGateF(), expo) * (scMax - scMin) + scMin;
    tick_t len        = (tick_t) (std::floor(valueMapped * getStepSize()));
    dbgassert(len > 0);
    return len;
}

int midiarp::isChordOutput() {
    return getPatternIdx() == 0;
}
int getStepIdx(int option, int step, int nNotes) {
    if (nNotes < 2) {
        return 0;
    }
    switch (option) {
        case 0:
            // 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3
            return step % nNotes;
        case 1:
            // 3, 2, 1, 0, 3, 2, 1, 0,
            return nNotes - 1 - (step % nNotes);
        case 2:
            // 0, 1, 2, 3, 3, 2, 1, 0, 0, 1, 2, 3
            return (step / nNotes) % 2 == 0 ? getStepIdx(0, step, nNotes) : getStepIdx(1, step, nNotes);
        case 3:
            // 0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 2
            return (step / (nNotes - 1)) % 2 == 0 ? step % (nNotes - 1) : (nNotes - 1 - (step % (nNotes - 1)));
        case 4:
            // 3, 2, 1, 0, 1, 2, 3, 2
            return getStepIdx(3, step + nNotes - 1, nNotes);
        case 5:
            return (step / (nNotes - 1)) % 2 == 0 ? (nNotes - 1 - (step % (nNotes - 1))) : step % (nNotes - 1);
        case 6:
            // 0, 1, 0, 2, 0, 3, 0, 1, 0, 2, 0, 3
            if (step % 2 == 0)
                return 0;
            return 1 + getStepIdx(0, step / 2, nNotes - 1);
        case 7:
            // 0, 1, 0, 2, 0, 3, 0, 2, 0, 1, 0, 2
            if (step % 2 == 0)
                return 0;
            return 1 + getStepIdx(3, step / 2, nNotes - 1);
        default:
            return 0;
    }
}
int midiarp::getArpStepIdx(int _step, int nNotes) {
    int32_t option = getPatternIdx();
    if (option > 0) {
        option--;
    }
    return getStepIdx(option, _step, nNotes);
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
    if (flags != FLG_PAR_UPDATE_USER) {
        return;
    }
    dbgassert(this->trackImpl->getTrack());
    track_t* track                = this->trackImpl->getTrack();
    automationlane_snapshot_t ref = toRef();
    parameter_ref_t p             = { track->projectIdx, ref.type, 0, idx };
    DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
}
void midiarp::createSnapshot(arp_snapshot& snapshot, const tracksnapshot_store_opts_t& opts) {
    if (opts.storePluginPreset) {
        snapshot.params.reserve(getNumParameters());
        visitParams([&snapshot](auto& mapEntry) {
            automatable_param_t& param = mapEntry.second;
            snapshot.params.push_back(param_snapshot_t{ param.idx, param.value });
        });
    }
    if (opts.storeAutomation) {
        storeAutomation(snapshot.automatedParams, this);
    }
}

template<typename T>
void cleanupMarkers(T& markers, tick_t tickTmNow) {
    if (!markers.empty()) {
        int markersSize = markers.size();
        auto i          = std::begin(markers);
        while (i != std::end(markers)) {
            int64_t spawnTick = (*i).time;
            int64_t timeSince = tickTmNow - spawnTick;
            if (timeSince > TICKS_QUARTER * 16 || markersSize > 100) {
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
    for (arp_note_t& note : this->heldOutputNotes) {
        if (note.isEnabled() && note.isHeld()) {
            noteEvents.emplace_back(note.pitch, note.velocity, 0, note.start(), false, false);
        }
        note.setEnabled(false);
        note.setIsHeld(false);
    }
    for (arp_note_t& note : this->heldInput) {
        if (note.isEnabled() && note.isHeld()) {
            noteEvents.emplace_back(note.pitch, note.velocity, 0, note.start(), false, false);
        }
    }
    heldInput.clear();
}

int32_t midiarp::getRandVelocity() {
    float fRandVel = pow(getRandVelocityF(), 2.0f);
    auto len     = (tick_t) (std::round(fRandVel * (127)));
    return len;
}

tick_t midiarp::getRandTime() {
    float fRandTm         = pow(getRandTimeF(), 2.0f);
    const int minDuration = 0;//math::max(0, getStepSize() >> 4);
    const int maxDuration = math::max(0, getStepSize() >> 1);
    auto len            = (tick_t) (std::floor(minDuration + fRandTm * (maxDuration - minDuration)));
    return len;
}

int midiarp::updateMarkersAndAnimation(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, float wallClockTime) {

    if (!heldOutputNotes.empty()) {
        auto i = std::begin(heldOutputNotes);
        while (i != std::end(heldOutputNotes)) {
            arp_note_t& heldNoteOut = *i;
            if (!heldNoteOut.isEnabled() && (wallClockTime - heldNoteOut.wallTime >= 1.0f)) {
                i = heldOutputNotes.erase(i);
            } else
                i++;
        }
    }
    cleanupMarkers(markers, end);
    cleanupMarkers(markers2, end);
    return 0;
}

/* shortens end of intersecting notes, does not remove any notes, instead looks for exact duplicates
 * returns: -1 if exact duplicate is present
 * otherwise the return value is a positive number and represents the number notes modified in the list
 */
template<typename T>
inline int cutNoteOutOfList(std::vector<T>& m_list, T& n, bool eliminateDupes) {
    auto it = m_list.begin();
    // find exact duplicate
    while (it != m_list.end()) {
        const T& val = *it++;
        if (val.isEnabled() && val.pitch == n.pitch && val.time == n.time && val.len == n.len) {
            return -1;
        }
    }

    int nAdjusted = 0;
    for (it = m_list.begin(); it != m_list.end(); ++it) {
        T& c = *it;
        if (!c.isEnabled())
            continue;
        if (c.pitch != n.pitch) {
            continue;
        } else if (c.start() >= n.end() || c.end() <= n.start()) {
            continue;
        } else if (c.start() < n.start()) {
            c.cutRight(n.start());
            nAdjusted++;
        } else if (c.start() > n.start()) {
            n.cutRight(c.start());
            nAdjusted++;
        }
    }
    return nAdjusted;
}
void midiarp::addNote(tick_t start, arp_note_t& note, std::vector<noteevent_t>& noteEvents) {
    //find overlapping held notes;
    int retVal = cutNoteOutOfList(heldOutputNotes, note, true);
    if (retVal == -1) {
        log_lf(Log::L_ERROR, "exact duplicate in list!\n");
        return;
    }
    if (retVal != 0) {
        if constexpr (logProcessedNotes) {
            log_lf(Log::L_DEBUG, "intersecting. ret val %d\n", retVal);
        }
    }
    heldOutputNotes.push_back(note);

    if (note.isHeld()) {
        if constexpr (logProcessedNotes) {
            log_lf(Log::L_DEBUG, "Block %d: %s ARP ON at %d = %d, arp enabled: %d\n", start, noteName(note.pitch), note.start() - start, note.start(), enable);
        }
        noteEvents.emplace_back(note.pitch, note.velocity, note.start() - start, note.start(), true, false);
    }
}

void midiarp::initRandomDelays(tick_t tick, tick_t startFrame, tick_t endFrame, int32_t nextStep, tick_t stepSize, uint64_t seed, bool reset) {
    uint64_t stepSeed_u64 = (((resetTime + nextStep * stepSize + (seed) *326597ULL) * 2825836522051561ULL + stepSize * 1285607ULL) + nextStep) * 55733ULL;
    arpRand.rng_seed(stepSeed_u64);
    velocitySeed_u64     = static_cast<uint64_t>(arpRand.rng_rand()) << 32 | arpRand.rng_rand();
    int32_t randMode     = getRandTmMode();
    bool noRandomOnReset = true;

    int32_t rndTime = noRandomOnReset && nextStep == 0 ? 0 : this->getRandTime();

    markers2.clear();
    processTimePoints.clear();
    for (int i = 0; i < NUM_ARP_MAX_POLY_VOICES; i++) {
        if (noRandomOnReset && nextStep == 0) {
            curRandTimeOffset[i] = 0;
        } else if (randMode == 0) {
            curRandTimeOffset[i] = (int32_t)arpRand.rng_rand(rndTime);
        } else {
            curRandTimeOffset[i] = -rndTime + (int32_t)arpRand.rng_rand(rndTime * 2);
        }
        tick_t rndStepTime = resetTime + nextStep * stepSize + curRandTimeOffset[i];
        processTimePoints.push_back(rndStepTime);

#ifdef PLACE_MARKERS
        if (i < 6) {
            String str = StringFormat("@%d processTimePoints[%d]", rndStepTime, i);
            markers2.push_back(marker_t{ rndStepTime, col(7), str, (float) (i) });
        }
#endif
    }
    if constexpr (logProcessedNotes) {
        log_lf(Log::L_DEBUG, "@%s STEP %d STEPSIZE %d FIRST %s SEED %016zx VEL-SEED %016zx rndTime %d\n",
                   StringAsCStr(tickAsBeatString(tick)),
                   nextStep, stepSize, StringAsCStr(tickAsBeatString(processTimePoints[0])), stepSeed_u64, velocitySeed_u64, rndTime);
    }
}

bool midiarp::isOutputNoteGateOn(const arp_note_t& noteHeldOut) {
    if (gateOutputNotes) {
        return std::find_if(heldInput.cbegin(), heldInput.cend(), [arpnoteUid = noteHeldOut.arpNoteUid](const arp_note_t& noteHeldIn) {
                   return noteHeldIn.arpNoteUid == arpnoteUid;
               }) != heldInput.cend();
    }
    return true;
}

int midiarp::endOutputNotes(tick_t tick, tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, std::vector<noteevent_t>& noteEventsProcessed) {
    if (heldOutputNotes.empty()) {
        return 0;
    }
    int nSend = 0;
    bool forceLoopEndNotesOff = loopEnd > -1 && tick + 1 >= loopEnd;
    for (arp_note_t& heldNoteOut : heldOutputNotes) {
        if (heldNoteOut.isEnabled() &&
            (forceLoopEndNotesOff || heldNoteOut.end() <= tick || heldNoteOut.end() < start || !isOutputNoteGateOn(heldNoteOut))) {
            auto tickOffsetInBlockEnd = math::min(end - start - 1, tick - start);

            if constexpr (logProcessedNotes) {
                if (heldNoteOut.isHeld()) {
                    if (forceLoopEndNotesOff) {
                        log_lf(Log::L_DEBUG, "Block %d-%d: %s ARP Force OFF (LOOP END %d) at %d = %d\n", start, end, noteName(heldNoteOut.pitch), loopEnd, tickOffsetInBlockEnd, tick);
                    } else {
                        log_lf(Log::L_DEBUG, "Block %d-%d: %s@%d ARP OFF at %d = %d\n", start, end, noteName(heldNoteOut.pitch), heldNoteOut.start(), tickOffsetInBlockEnd, tick);
                    }
                }
            }
            if (tickOffsetInBlockEnd < 0) {
                dbgassert(heldNoteOut.end() <= start);
                log_lf(Log::L_ERROR, "ending output note with end() < blockStart\n");
                tickOffsetInBlockEnd = 0;
            }
            heldNoteOut.cutRight(tick);
#ifdef PLACE_MARKERS
            markers.push_back(marker_t{ tick, col(5), StringFormat("%d end %s start %d len %d end %d tickoffset %d", tick, noteName(heldNoteOut.pitch), heldNoteOut.start(), heldNoteOut.len, heldNoteOut.end(), tickOffsetInBlockEnd), (float) (tickMarkers++) });
#endif
            if (heldNoteOut.isHeld()) {
                noteEventsProcessed.emplace_back(heldNoteOut.pitch, heldNoteOut.velocity, tickOffsetInBlockEnd, heldNoteOut.start(), false, forceLoopEndNotesOff);
                nSend++;
            }
            heldNoteOut.setIsHeld(false);
            heldNoteOut.setEnabled(false);
        }
    }
    return nSend;
}

bool midiarp::isProcessingEnabled() {
    if (!this->enable) {
        const automation_t* automatEnable = getRegisteredConstAutomation(PARAM_ENABLE);
        if (!automatEnable || !automatEnable->active) {
            return false;
        }
    }
    return true;
}

void midiarp::process(playback_state state, tick_t cursorPos, const std::vector<noteevent_t>& noteEventsIn,
                      tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd,
                      std::vector<noteevent_t>& noteEventsProcessed) {
    const float wallClockTime = getTimeMillisF() / 1000.0f;

    if (!isProcessingEnabled() && !bEnableStateUserToggled && heldInput.empty() && heldOutputNotes.empty() && noteEventsIn.empty()) {
        noteEventsProcessed = noteEventsIn;
    } else {
        processArpInternal(state, cursorPos, noteEventsIn, start, end, loopStart, loopEnd, wallClockTime, noteEventsProcessed);
    }

    updateMarkersAndAnimation(start, end, loopStart, loopEnd, wallClockTime);
    /*if (wallClockTime - tmLastLog > 10.0f) {
        tmLastLog = wallClockTime;
        log_lf(Log::L_DEBUG, "%zu/%zu/%zu/%zu/%zu/%zu\n",
                   heldInput.size(),
                   heldOutputNotes.size(),
                   curRandTimeOffset.size(),
                   processTimePoints.size(),
                   markers.size(),
                   markers2.size());
    }*/
}

/*
 * midiarp::processArpInternal
 * Note: notes that are held (= sent out) are flagged IS_HELD
 * heldOutputNotes only contains pattern generated notes, not pass thru input notes
 * heldOutputNotes may contain disabled notes: They are released notes that will remain for a constant time for GUI animation purposes
 */
void midiarp::processArpInternal(playback_state state, tick_t cursorPos, const std::vector<noteevent_t>& noteEventsIn,
                                 tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, float wallClockTime,
                                 std::vector<noteevent_t>& noteEventsProcessed) {

    tickMarkers = 0;

#ifdef PLACE_MARKERS
    markers.push_back(marker_t{ start, col(7), StringFormat("process %d evts", noteEventsIn.size()), (float) (tickMarkers++) });
#endif
#ifdef PLACE_MARKERS
    markers.push_back(marker_t{ end, col(7), "block end", (float) (tickMarkers++) });
#endif
    std::vector<automated_param_t> arpAutomatedParams;
    getAllAutomatedParams(arpAutomatedParams);

    std::vector<noteevent_t> noteEvents = noteEventsIn;
    sortNoteEvents(noteEvents);

    int nSend = 0;
    size_t eventIdx = 0;
    size_t evtsProcessed = 0;

    const tick_t arpLoopStart = loopStart > -1 ? math::max(loopStart, start) : start;
    const tick_t arpLoopEnd   = loopEnd > -1 ? math::min(loopEnd, end) : end;
    for (tick_t t = arpLoopStart; t < arpLoopEnd; t++) {
        const tick_t tick = t;

        bool enabledBefore    = this->enable;
        tick_t stepSizeBefore = getStepSize();
        if (bEnableStateUserToggled) {
            bEnableStateUserToggled = false;
            this->enable            = bEnableNextState;
        }

        tick_t automationPos = tick;
        if (!DAW::isPlaybackState(state)) {
            automationPos = cursorPos;
        }
        for (automated_param_t& param : arpAutomatedParams) {
            if (param.src.isActive()) {
                float val = param.src.getValueAt(automationPos);
                setParamValue(param.paramIdx, val, FLG_PAR_UPDATE_AUTOMATED);
            }
        }

        tick_t stepSize           = getStepSize();
        tick_t noteDuration       = getDuration();
        const bool isChordPattern = isChordOutput();

        // handle on/off state changes
        if (this->enable != enabledBefore) {
            if constexpr (logProcessedNotes) {
                log_lf(Log::L_DEBUG, "Block %d: ARP STATE CHANGED TO (%s) at %d = %d, %zu heldinput, %zu heldOutput\n", start, (enable ? "enabled" : "disabled"), tick - start, tick, heldInput.size(), heldOutputNotes.size());
            }
            // end incoming notes when arp was enabled. Restart incoming notes when arp has been disabled
            for (arp_note_t& noteInHeld : this->heldInput) {
                if (enable == noteInHeld.isHeld()) {
                    noteInHeld.setIsHeld(!enable);
                    noteevent_t noteEvt(noteInHeld.pitch, noteInHeld.velocity, tick - start, tick, !enable, false);
                    noteEventsProcessed.push_back(noteEvt);
                    if constexpr (logProcessedNotes) {
                        log_lf(Log::L_DEBUG, "Block %d: %s ARP PASSTHRU heldIn %s at %d = %d\n", start, noteName(noteInHeld.pitch), (!enable ? "ON" : "OFF"), tick - start, tick);
                    }
                    nSend++;
                }
            }

            // Start outgoing notes if arp was enabled this tick.
            // End outgoing notes if arp has been disabled this tick.
            for (arp_note_t& noteOutHeld : this->heldOutputNotes) {
                if (noteOutHeld.isEnabled()) {
                    if (enable != noteOutHeld.isHeld()) {
                        noteOutHeld.setIsHeld(enable);
                        noteevent_t noteEvt(noteOutHeld.pitch, noteOutHeld.velocity, tick - start, tick, enable, false);
                        noteEventsProcessed.push_back(noteEvt);
                        if constexpr (logProcessedNotes) {
                            log_lf(Log::L_DEBUG, "Block %d: %s ARP PASSTRU heldOut %s at %d = %d\n", start, noteName(noteOutHeld.pitch), (enable ? "ON" : "OFF"), tick - start, tick);
                        }
                        nSend++;
                    }
                }
            }
        }


        // TODO: store index of last processed event and start iterating at that location
        while (eventIdx < noteEvents.size()) {
            const noteevent_t& evt = noteEvents[eventIdx];
            const tick_t evtTick = evt.tickOffsetInBlock + start;
            if (evtTick > tick) {
                break;
            }
            ++eventIdx;
            if (evtTick < tick) {
                continue;
            }
            ++evtsProcessed;

            // process note off event (even when arp is disabled)
            if (!evt.isNoteOn) {
                auto it = std::find_if(heldInput.begin(), heldInput.end(), [&evt](const auto& heldArpIn) {
                    return evt.pitch == heldArpIn.pitch;
                });
                if (it == heldInput.end()) {
                    // arp received a note off with the corresponding note_on missing
                    log_lf(Log::L_ERROR, "Arp received note off with the corresponding note_on missing %s\n", noteName(evt.pitch));
                } else {
                    arp_note_t& arpInputNote = *it;
                    if constexpr (logProcessedNotes) {
                        log_lf(Log::L_DEBUG, "Block %d: %s ARP INPUT OFF (HELD %s) at %d = %d\n", start, noteName(arpInputNote.pitch), (arpInputNote.isHeld() ? "ON" : "OFF"), tick - start, tick);
                    }
#ifdef PLACE_MARKERS
                    markers.push_back(marker_t{ tick, col(5), StringFormat("END IN %s", noteName((*it).pitch)), (float) (tickMarkers++) });
#endif
                    heldInput.erase(it);
                }
            }

            // process note on event (even when arp is disabled)
            if (evt.isNoteOn) {
                auto it = std::find_if(heldInput.begin(), heldInput.end(), [&evt](const auto& heldArpIn) {
                    return evt.pitch == heldArpIn.pitch;
                });
                if (it != heldInput.end()) {
                    log_lf(Log::L_ERROR, "Arp received double note on event %s\n", noteName(evt.pitch));
                    log_lf(Log::L_ERROR, "New event is at tick %d\n", evt.tickOffsetInBlock + start);
                    log_lf(Log::L_ERROR, "Held note started at tick %d\n", it->time);
                }
                if (it == heldInput.end()) {
                    if (heldInput.empty()) {
                        reset(tick);
#ifdef PLACE_MARKERS
                        markers.push_back(marker_t{ tick, col(1), StringFormat("Step %d reset", step), (float) (tickMarkers++) });
#endif
                        initRandomDelays(tick, start, end, 0, stepSize, lSeed, true);
                        stepGenerated = 0;
                    }
                    arp_note_t arpInputNote;
                    arpInputNote.time     = tick;
                    arpInputNote.pitch    = evt.pitch;
                    arpInputNote.velocity = evt.velocity;
                    arpInputNote.len      = TICKS_QUARTER * 2;
                    arpInputNote.len      = 0;
                    arpInputNote.setIsHeld(!enable);
                    arpInputNote.setEnabled(true);
                    arpInputNote.arpNoteUid = this->arpNoteUidCounter++;
                    if constexpr (logProcessedNotes) {
                        log_lf(Log::L_DEBUG, "Block %d: %s ARP INPUT ON (HELD %s) at %d = %d\n", start, noteName(arpInputNote.pitch), (arpInputNote.isHeld() ? "ON" : "OFF"), tick - start, tick);
                    }
                    heldInput.push_back(arpInputNote);
#ifdef PLACE_MARKERS
                    markers.push_back(marker_t{ arpInputNote.time, col(5), StringFormat("Note Start IN %s", noteName(arpInputNote.pitch)), (float) (tickMarkers++) });
#endif

                } else {
#ifdef PLACE_MARKERS
                    markers.push_back(marker_t{ evt.tickOffsetInBlock + start, col(1), StringFormat("Note Duplicate Held IN %s", noteName(evt.pitch)), (float) (tickMarkers++) });
#endif
                }
            }

            // arp is disabled: pass thru events
            if (!enable) {
                if constexpr (logProcessedNotes) {
                    log_lf(Log::L_DEBUG, "Block %d: %s ARP PASSTRU evt %s at %d = %d\n", start, noteName(evt.pitch), (evt.isNoteOn ? "ON" : "OFF"), evt.tickOffsetInBlock, evt.globalTick);
                }
                noteEventsProcessed.push_back(evt);
                nSend++;
            }
        }

        const auto stepRecalc = (tick - resetTime) / stepSize;


        if constexpr (logProcessedNotes) {
            if (tick >= resetTime && (tick - resetTime) % stepSize == 0) {
                if (DAW::isPlaybackState(state)) {
#ifdef PLACE_MARKERS
                    markers.push_back(marker_t{ tick, col(3), StringFormat("Increment step %d tickMarkers %d", step, tickMarkers), (float) (tickMarkers++) });
#endif
                    log_lf(Log::L_DEBUG, "@%s first tick in step %d\n",
                               StringAsCStr(tickAsBeatString(tick)),
                               stepRecalc);
                }
            }
            if (stepSizeBefore != stepSize) {
                const auto stepCurrent = (tick - resetTime) / stepSizeBefore;
                log_lf(Log::L_DEBUG, "@%s STEPSIZE change from %d to %d. Step: %d recalculated: %d\n",
                           StringAsCStr(tickAsBeatString(tick)),
                           stepSizeBefore, stepSize, stepCurrent, stepRecalc);
            }
        }

        // run reset logic (even when arp is disabled). pregenerates the randomized step timings for the next step
        if (!heldInput.empty()) {
            int stepToGenerate = -1;
            if (tick >= resetTime && (tick - resetTime) % stepSize == 0 && stepGenerated != stepRecalc) {
                stepToGenerate = stepRecalc;
            } else if ((stepGenerated != stepRecalc + 1) && std::all_of(std::cbegin(processTimePoints), std::cend(processTimePoints), [tick](auto t) {
                           return t < tick;
                       })) {
                stepToGenerate = stepRecalc + 1;
            }
            if (stepToGenerate > -1) {

#ifdef PLACE_MARKERS
                markers.push_back(marker_t{ tick, col(2), StringFormat("Generate step %d (prev %d, exact %d)", step + 1, stepGenerated), (float) (tickMarkers++) });
#endif

                initRandomDelays(tick, start, end, stepToGenerate, stepSize, lSeed, false);
                bool stepCompleted2 = std::all_of(std::cbegin(processTimePoints), std::cend(processTimePoints), [tick](auto t) {
                    return t < tick;
                });
                if (stepCompleted2) {
                    log_lf(Log::L_WARN, "all generated timepoints are before the current tick\n");
                }
                auto stepSomeCompleted2 = std::count_if(std::cbegin(processTimePoints), std::cend(processTimePoints), [tick](auto t) {
                    return t < tick;
                });
                if (stepSomeCompleted2) {
                    log_lf(Log::L_WARN, "some steps (%zd) generated lay before current tick (at step %d range %s %s)\n", stepSomeCompleted2, stepToGenerate, StringAsCStr(tickAsBeatString(start)), StringAsCStr(tickAsBeatString(end)));
                }
                stepGenerated = stepToGenerate;
            }
        }

        // generate heldOutput notes even when arp is disabled
        if (processTimePoints.size() != NUM_ARP_MAX_POLY_VOICES && !heldInput.empty()) {
            log_lf(Log::L_WARN, "processTimePoints.size() is %zu, waiting for reset...\n", processTimePoints.size());
        }
        auto maxProcPts = math::min<size_t>(isChordPattern ? NUM_ARP_MAX_POLY_VOICES : 1, heldInput.size());
        maxProcPts      = math::min<size_t>(maxProcPts, processTimePoints.size());

        // only in polyphonic chord mode this loop will iterate past 0
        for (size_t prIdx = 0; prIdx < maxProcPts; prIdx++) {
            tick_t timeStepPreGenerated = processTimePoints[prIdx];
            if (timeStepPreGenerated != tick)
                continue;
            int actualStep = stepGenerated;
            if (stepGenerated < 0) {
                log_lf(Log::L_ERROR, "stepGenerated < 0, unexpected...\n");
                actualStep = 0;
            }
#ifdef PLACE_MARKERS
            markers.push_back(marker_t{ timeStepPreGenerated, col(2347), StringFormat("stp prIdx %d heldInNotes %d heldOutNotes %d", prIdx, heldInput.size(), heldOutputNotes.size()), (float) (tickMarkers++) });
#endif

            int noteStpIdx = -1;
            if (isChordPattern && prIdx < heldInput.size()) {
                noteStpIdx = prIdx;
            } else if (!isChordPattern && !heldInput.empty()) {
                noteStpIdx = getArpStepIdx(actualStep, CtrSize(heldInput));
            }
            if (noteStpIdx > -1) {
                const arp_note_t& noteArpInput = heldInput[noteStpIdx];
                arp_note_t noteArpStep         = noteArpInput;
                noteArpStep.setIsHeld(enable);
                noteArpStep.setEnabled(true);
                noteArpStep.time        = tick;
                noteArpStep.len         = noteDuration;
                noteArpStep.wallTime    = wallClockTime;
                int32_t rndVelIntensity = this->getRandVelocity();
                if (rndVelIntensity) {
                    uint64_t stepSeed_u64 = (velocitySeed_u64 + noteArpInput.time * 2888443ULL + noteArpInput.pitch * 341123ULL + stepRecalc) * 484751ULL;
                    arpRand.rng_seed(stepSeed_u64);
                    tick_t randVel       = -rndVelIntensity + static_cast<int32_t>(arpRand.rng_rand(rndVelIntensity * 2));
                    noteArpStep.velocity = math::clamp(noteArpStep.velocity + randVel, 1, 127);
                }
                addNote(start, noteArpStep, noteEventsProcessed);
                nSend++;
#ifdef PLACE_MARKERS_OUTPUT
                String str = StringFormat("note %d %d %s", noteArpStep.time, noteArpStep.len, noteName(noteArpStep.pitch));
                markers.push_back(marker_t{ tick, col(2), str, (float) (tickMarkers++) });
#endif
            }
        }

        // process ending output notes (even when arp is disabled)
        nSend += endOutputNotes(tick, start, end, loopStart, loopEnd, noteEventsProcessed);
    }

    // sanity check, remove later on
    if (evtsProcessed != noteEventsIn.size()) {
        log_lf(Log::L_ERROR, "Block %d-%d: ARP did not process all events. Processed %zu of %zu\n", start, end, evtsProcessed, noteEventsIn.size());
        log_lf(Log::L_ERROR, "Block %d-%d: loopStart %d, loopEnd %d\n", start, end, loopStart, loopEnd);
        for (noteevent_t& evt : noteEvents) {
            log_lf(Log::L_ERROR, "Block %d-%d: Event %s %d %d %s\n", start, end, evt.isNoteOn ? "ON" : "OFF", evt.globalTick, evt.tickOffsetInBlock, noteName(evt.pitch));
        }
    }

    // run endOutputNotes for one past the end tick, this might be the loopEnd tick or blockend tick
    nSend += endOutputNotes(arpLoopEnd, start, end, loopStart, loopEnd, noteEventsProcessed);

    if (nSend)
        sortNoteEvents(noteEventsProcessed);
}
