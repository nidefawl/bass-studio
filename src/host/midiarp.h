#pragma once
#include "assert_dbg.h"
#include "automation.h"
#include "host/host.h"
#include "logging.h"
#include "math/seq_math.h"
#include "note.h"
#include "platform.h"
#include "seq_time.h"
#include "str_util.h"
#include "track_impl.h"
#include <algorithm>
#include <array>
#include <iterator>
#include "types.h"
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

#ifndef NDEBUG
#define DAW_DEBUG_ARP 
#endif

struct arp_snapshot;
namespace DAW {

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
    struct arp_param_entry_t {
        int32_t id = 0;
        String name;
        String unit;
        float val = 0.0f;
        int32_t quantizationSteps = 0;
    };
    static const std::array<tick_t, 16 * 3> tickLength;
    static const std::array<arp_param_entry_t, 8> parameterTypes;
private:
    std::vector<arp_note_t> heldInput;
    std::vector<arp_note_t> heldOutputNotes;
    std::vector<tick_t> curRandTimeOffset;
    std::vector<tick_t> processTimePoints;
    track_impl_t* const trackImpl;
    // ResetMode resetMode       = ResetMode::NOTE;
    bool enable               = false;
    int32_t step              = 0;
    int32_t stepGenerated     = 0;
    int32_t arpNoteUidCounter = 1;
    tick_t resetTime          = 0;
    uint64_t lSeed            = 13L;
    uint64_t velocitySeed_u64 = 326597L;
    seq_rand arpRand;
    int tickMarkers              = 0;
    bool gateOutputNotes         = true;
    bool syncClock               = true;
    bool bEnableStateUserToggled = false;
    bool bEnableNextState        = false;
    // float tmLastLog              = 0.0f;

    void initRandomDelays(tick_t tick, tick_t startFrame, tick_t endFrame, int32_t nextStep, tick_t stepSize, uint64_t seed, bool reset);
    bool isOutputNoteGateOn(const arp_note_t& noteHeldOut);
    void addNote(tick_t start, arp_note_t& note, std::vector<midievent_note_t>& noteEvents);
    void processArpInternal(const DAW::Host::PluginManager* const host, playback_state state, tick_t cursorPos, const std::vector<midievent_note_t>& noteEventsIn,
                            tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, float wallClockTime,
                            std::vector<midievent_note_t>& noteEventsProcessed);
    int updateMarkersAndAnimation(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, float wallClockTime);
    int endOutputNotes(tick_t tick, tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, std::vector<midievent_note_t>& noteEventsProcessed);

public:
    std::vector<marker_t> markers;
    std::vector<marker_t> markers2;

#ifdef DAW_DEBUG_ARP
    std::array<int32_t, 128> debugNoteCounts{};
    std::array<int32_t, 128> prevDebugNoteCounts{};
    Host::note_event_validator_t inputValidator;
#endif

    explicit midiarp(track_impl_t* _trImpl);
    ~midiarp() override = default;
    track_t* getTrack() override {
        dbgassert(this->trackImpl);
        return this->trackImpl->getTrack();
    }
    const std::vector<arp_note_t>& getHeldNotes() {
        return this->heldOutputNotes;
    }
    float getGateF() {
        return getParamValue(ARP_PARAM_GATE);
    }
    float getRandTimeF() {
        return getParamValue(ARP_PARAM_RAND_TIME);
    }
    float getRandVelocityF() {
        return getParamValue(ARP_PARAM_RAND_VEL);
    }
    int getPatternIdx(float f) {
        return math::clamp(math::floorfS32(f * (NUM_PATTERNS)), 0, NUM_PATTERNS - 1);
    }

    tick_t getStepSize(float f);
    int32_t getRandTmMode(float f);
    tick_t getDuration(float f);
    int32_t getRandVelocity(float f);
    tick_t getRandTime(float f);

    int isChordOutput();
    int getArpStepIdx(int _step, int nNotes);


    float getStepSizeParamValueFromMapped(tick_t len);
    float getDurationParamValueFromMapped(tick_t len);
    float getRandTimeParamValueFromMapped(tick_t len);
    float getRandVelocityParamValueFromMapped(int32_t vel);

    String getAutomatableName() override {
        return "Arp";
    }
    void reset(tick_t _resetTime);
    void allNotesOff(std::vector<midievent_note_t>& noteEvents);
    void onStartPlayback();

    void updateAutomatedParameters(const Host::PluginManager* const host, tick_t tick, playback_state state) override;

    automatable_param_ref_t toRef() const override {
        automatable_param_ref_t ref;
        ref.type  = AUTOMATABLE_ARP;
        ref.refId = static_cast<int32_t>(trackImpl->stageId.stageId);
        return ref;
    }
    void createSnapshot(arp_snapshot& snapshot, const tracksnapshot_store_opts_t& opts);
    void loadSnapshot(const arp_snapshot& snapshot);

    void process(const Host::PluginManager* const host, playback_state state, tick_t cursorPos, const std::vector<midievent_note_t>& noteEventsIn,
                 tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd,
                 std::vector<midievent_note_t>& noteEventsProcessed);
    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
    bool isProcessingEnabled();

    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;

};
}
