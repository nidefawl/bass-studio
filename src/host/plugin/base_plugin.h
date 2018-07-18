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

struct AudioBlock;
struct handles_t;
class track_t;
class guibase;
struct track_impl_t;

class effectbase : public automatable_t {
public:
	rmsmeter<16000> meter;
	AudioBlock* blockInputs = NULL; // guaranteed to have at least 2 channels
	AudioBlock* blockOutputs = NULL; // guaranteed to have at least 2 channels
	int32_t projectGlobalId;
	bool bIsEnabled = false;
	bool bIsSetup = false;
	bool bCanReceiveMidi = false;
	effectbase(int32_t _projectGlobalId) : projectGlobalId(_projectGlobalId) {
	}
	virtual ~effectbase() {
	}
	virtual void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) = 0;
	virtual guibase* makeGui() = 0;
	virtual void setSlot(int32_t i) = 0;
	virtual int32_t getSlot() = 0;
	virtual void breakTrackLink() = 0;
	virtual void setTrackLink(track_impl_t* trImpl) = 0;
	virtual track_impl_t* getTrackLink() = 0;
	virtual guibase* getGui() = 0;
	virtual void process(AudioBlock* in, AudioBlock* out, int32_t samples) = 0;
	virtual bool show() = 0;
	virtual bool close() = 0;
	virtual int32_t getDelay() = 0;
	virtual bool hasParam(int32_t idx) = 0;
	virtual automated_param_t* getRegisteredAutomation(int32_t idx) = 0;
	virtual String getInfo(std::vector<String>& list) = 0;
};


