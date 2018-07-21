#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "str_util.h"
#include "seq_time.h"
#include "automation.h"
#include "logging.h"
#include "platform.h"
#include "meter.h"
#include "snapshot.h"
#include "modules.h"

struct AudioBlock;
struct handles_t;
class track_t;
class guiplugin;
class vsthost;
struct audio_stage_t;
struct plugin_snapshot_t;

class effectbase : public automatable_t {
public:
	rmsmeter<16000> meter;
	AudioBlock* blockInputs = NULL; // guaranteed to have at least 2 channels
	AudioBlock* blockOutputs = NULL; // guaranteed to have at least 2 channels
	const int32_t pluginType = 0;
	int32_t projectGlobalId;
	bool bIsEnabled = false;
	bool bIsSetup = false;
	bool bCanReceiveMidi = false;
	audio_stage_t* trackImpl = nullptr;
	int32_t slot = -1;
	effectbase(int32_t _pluginType, int32_t _projectGlobalId) : pluginType(_pluginType), projectGlobalId(_projectGlobalId) {
	}
	virtual ~effectbase() {
	}
	virtual void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) = 0;
	virtual guiplugin* makeGui() = 0;
	virtual guiplugin* getGui() = 0;
	virtual void process(AudioBlock* in, AudioBlock* out, int32_t samples) = 0;
	virtual bool show() = 0;
	virtual bool close() = 0;
	virtual bool resume() = 0;
	virtual bool sleep() = 0;
	virtual void unload() = 0;
	virtual void load(vsthost* host) = 0;
	virtual int32_t getDelay() = 0;
	virtual bool hasParam(int32_t idx) = 0;
	virtual automated_param_t* getRegisteredAutomation(int32_t idx) = 0;
	virtual String getInfo(std::vector<String>& list) = 0;
	track_t* getTrack();
	virtual void onTick(double since);
	virtual void loadSnapshot(const plugin_snapshot_t& snapshot) = 0;

	virtual void breakTrackLink();
	virtual void setTrackLink(audio_stage_t* audioStage);
	virtual void onPreUnload() {

	}
	virtual void setSlot(int32_t i) {
		slot = i;
	}
	virtual int32_t getSlot() {
		return slot;
	}
	virtual audio_stage_t* getTrackLink() {
		return trackImpl;
	}
};


