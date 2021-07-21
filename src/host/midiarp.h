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


#define ARP_PARAM_CLOCK PARAM_OFFSET_IMPL
#define ARP_PARAM_GATE (PARAM_OFFSET_IMPL+1)
#define ARP_PARAM_PATTERN (PARAM_OFFSET_IMPL+2)
#define ARP_PARAM_RAND_TIME (PARAM_OFFSET_IMPL+3)
#define ARP_PARAM_RAND_MODE (PARAM_OFFSET_IMPL+4)
#define ARP_PARAM_RAND_VEL (PARAM_OFFSET_IMPL+5)
#define NUM_ARP_STEPSIZE_OPTIONS 16
#define NUM_PATTERNS 6
#define NUM_RANDOM_TIME_MODES 2
#define NUM_ARP_MAX_POLY_VOICES 32

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
	ResetMode resetMode = ResetMode::NOTE;
	std::vector<noteevent_t> heldInput;
	std::vector<noteevent_t> heldOutput;
	std::vector<tick_t> curRandTimeOffset;
	std::vector<tick_t> processTimePoints;
	std::vector<int32_t> processNotesSpawn;
public:
	std::vector<note_t> heldOutputNotes;
	std::vector<note_t> heldInputAnimationNotes;
	std::vector<note_t> heldOutputAnimationNotes;
	std::vector<marker_t> markers;
	std::vector<marker_t> markers2;
	std::vector<int64_t> notesSpawnTime;
private:
	int32_t step = 0;
	int32_t stepGenerated = 0;
	tick_t resetTime = 0;
	tick_t lastStepSize = 0;
	int noteIdx = 0;
	std::array<tick_t, 16*3> tickLength;
	int32_t numCalls = 0;
	track_impl_t* const trackImpl;
    int maxNoteChordCount = 6;
    long lSeed = 13L;
    seq_rand arpRand;
	int tickMarkers = 0;

    void initRandomDelays(uint64_t seed, int32_t step, int32_t stepSize, int32_t startFrame, int32_t endFrame, bool reset);

public:
	bool enable = false;
	midiarp(track_impl_t* _trImpl);
	~midiarp() {

	}
	void reset(tick_t _resetTime) {
		noteIdx = 0;
		resetTime = _resetTime;
        step = 0;
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
	float getPatternF() {
		return getParamValue(ARP_PARAM_PATTERN);
	}
	float getClockF() {
		return getParamValue(ARP_PARAM_CLOCK);
	}
	float getGainF() {
		return getParamValue(PARAM_GAIN);
	}

	tick_t getStepSize();
	int32_t getRandTmMode();
	tick_t getDuration();
	int32_t getRandVelocity();

	tick_t getRandTime();
	int isChordOutput();
	int getStepIdx(int step, int nNotes);

	void addNote(std::vector<noteevent_t>& noteEvents, tick_t start, note_t& note, int64_t time);


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
				param->inUse = true;
			}
		}
	}
	automationlane_snapshot_t toRef() const override {
		automationlane_snapshot_t ref;
		ref.type = AUTOMATABLE_ARP;
		ref.refId = static_cast<int32_t>(trackImpl->stageId.stageId);
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
					std::vector<noteevent_t>& noteEventsProcessed, tick_t ticksPerBlock);
	int writeOutputNotes(std::vector<noteevent_t>& noteEventsProcessed,
			tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int64_t time);
	void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
	int endOutputNotes(tick_t tick, tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, std::vector<noteevent_t>& noteEventsProcessed);
};

