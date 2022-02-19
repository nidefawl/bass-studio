#pragma once
#include "assert_dbg.h"
#include "automation.h"
#include "logging.h"
#include "note.h"
#include "platform.h"
#include "seq_time.h"
#include "str_util.h"
#include "track_impl.h"
#include <algorithm>
#include <array>
#include <iterator>
#include <cstdint>
#include <vector>


#define ARP_PARAM_CLOCK PARAM_OFFSET_IMPL
#define ARP_PARAM_GATE (PARAM_OFFSET_IMPL + 1)
#define ARP_PARAM_PATTERN (PARAM_OFFSET_IMPL + 2)
#define ARP_PARAM_RAND_TIME (PARAM_OFFSET_IMPL + 3)
#define ARP_PARAM_RAND_MODE (PARAM_OFFSET_IMPL + 4)
#define ARP_PARAM_RAND_VEL (PARAM_OFFSET_IMPL + 5)
#define NUM_ARP_STEPSIZE_OPTIONS 16
#define NUM_PATTERNS (1 + 8)
#define NUM_RANDOM_TIME_MODES 2
#define NUM_ARP_MAX_POLY_VOICES 32

struct arp_snapshot;

struct arp_note_t : note_t {
    int32_t arpNoteUid = 0;
    float wallTime     = 0.0f;
};
class midiarp : public automatable_t {
public:
    enum ResetMode : int {
        NOTE,
        BEAT
    };

private:
    struct arp_param_entry_t {
        int32_t id = 0;
        String name;
        float val = 0.0f;
    };
    ResetMode resetMode = ResetMode::NOTE;
    std::vector<arp_note_t> heldInput;
    std::vector<arp_note_t> heldOutputNotes;
    std::vector<tick_t> curRandTimeOffset;
    std::vector<tick_t> processTimePoints;

public:
    std::vector<marker_t> markers;
    std::vector<marker_t> markers2;

private:
    int32_t step              = 0;
    int32_t stepGenerated     = 0;
    int32_t arpNoteUidCounter = 1;
    tick_t resetTime          = 0;
    std::array<tick_t, 16 * 3> tickLength{};
    track_impl_t* const trackImpl;
    uint64_t lSeed            = 13L;
    uint64_t velocitySeed_u64 = 326597L;
    seq_rand arpRand;
    int tickMarkers              = 0;
    bool gateOutputNotes         = true;
    bool syncClock               = true;
    bool bEnableStateUserToggled = false;
    bool bEnableNextState        = false;

    void initRandomDelays(tick_t tick, tick_t startFrame, tick_t endFrame, int32_t nextStep, tick_t stepSize, uint64_t seed, bool reset);

public:
    bool enable = false;
    explicit midiarp(track_impl_t* _trImpl);
    ~midiarp() override = default;
    void reset(tick_t _resetTime) {
        resetTime     = _resetTime;
        step          = 0;
        stepGenerated = -1;
    }
    void allNotesOff(std::vector<noteevent_t>& noteEvents);
    void onStartPlayback();
    float getGateF() {
        return getParamValue(ARP_PARAM_GATE);
    }
    float getRandTimeF() {
        return getParamValue(ARP_PARAM_RAND_TIME);
    }
    float getRandVelocityF() {
        return getParamValue(ARP_PARAM_RAND_VEL);
    }
    int getPatternIdx() {
        auto option = (int32_t) std::floor(getParamValue(ARP_PARAM_PATTERN) * (NUM_PATTERNS - 1));
        dbgassert(option < NUM_PATTERNS);
        return option;
    }

    tick_t getStepSize();
    int32_t getRandTmMode();
    tick_t getDuration();
    int32_t getRandVelocity();

    tick_t getRandTime();
    int isChordOutput();
    int getArpStepIdx(int _step, int nNotes);


    String getAutomatableName() override {
        return "Arp";
    }
    float getParamValue(int32_t idx) override {
        automatable_param_t* param = getParamUnchecked(idx);
        dbgassert(param);
        return param->value;
    }

    void updateAutomatedParameters(tick_t pos) override {
        // parameter automation updates happen are done internally
    }

    void setParamValue(int32_t idx, float val, int flags) override {
        automatable_param_t* param = getParamUnchecked(idx);
        dbgassert(param);
        param->value = val;
        if (param->idx == PARAM_ENABLE) {
            if ((flags & FLG_PAR_UPDATE_USER)) {
                bEnableNextState        = val > 0;
                bEnableStateUserToggled = true;
            } else {
                enable = val > 0;
            }
        }
        if (!(flags & FLG_PAR_UPDATE_NOSTORE) && !(flags & FLG_PAR_UPDATE_AUTOMATED)) {
            param->inUse = true;
        }
    }

    automationlane_snapshot_t toRef() const override {
        automationlane_snapshot_t ref;
        ref.type  = AUTOMATABLE_ARP;
        ref.refId = static_cast<int32_t>(trackImpl->stageId.stageId);
        return ref;
    }
    track_t* getTrack() override {
        dbgassert(this->trackImpl);
        return this->trackImpl->getTrack();
    }
    void createSnapshot(arp_snapshot& snapshot);
    void loadSnapshot(const arp_snapshot& snapshot);

    void process(playback_state state, tick_t cursorPos, const std::vector<noteevent_t>& noteEventsIn,
                 tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd,
                 std::vector<noteevent_t>& noteEventsProcessed);
    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
    bool isProcessingEnabled();
    const std::vector<arp_note_t>& getHeldNotes() {
        return this->heldOutputNotes;
    }

protected:
    bool isOutputNoteGateOn(const arp_note_t& noteHeldOut);
    void addNote(tick_t start, arp_note_t& note, std::vector<noteevent_t>& noteEvents);
    void processArpInternal(playback_state state, tick_t cursorPos, const std::vector<noteevent_t>& noteEventsIn,
                            tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, float wallClockTime,
                            std::vector<noteevent_t>& noteEventsProcessed);
    int updateMarkersAndAnimation(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, float wallClockTime);
    int endOutputNotes(tick_t tick, tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, std::vector<noteevent_t>& noteEventsProcessed);
};
