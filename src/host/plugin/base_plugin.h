#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <memory>
#include "str_util.h"
#include "seq_time.h"
#include "automation.h"
#include "logging.h"
#include "platform.h"
#include "meter.h"
#include "snapshot.h"
#include "modules.h"
#include "saferef.h"

struct AudioBlock;
struct handles_t;
class track_t;
class guiplugin;
class vsthost;
struct audio_stage_t;
struct plugin_snapshot_t;
struct plugin_snapshot_t;
class effect_deferred;

class effectbase : public automatable_t {
	SafeRef<effectbase> safeRef;
#ifndef NDEBUG
	//helper indicator in gdb.
	//gdb cannot display std::string when built without clib-debug flag (SLOW)
	const char* szName = NULL;
#endif
	int nLoadCalls = 0;
public:
	rmsmeter<16000> meter;
	AudioBlock* blockInputs = NULL; // guaranteed to have at least 2 channels
	AudioBlock* blockOutputs = NULL; // guaranteed to have at least 2 channels
	int32_t pluginType = 0;
	int32_t projectGlobalId;
	bool bIsEnabled = false;
	bool bIsSetup = false;
	bool bCanReceiveMidi = false;
	bool isSynth = false;
	String sName;
	String sProductName;
	audio_stage_t* trackImpl = nullptr;
	int32_t slot = -1;
	std::unique_ptr<DelayLine> delayLine;
	double fTimePercentBlockProcess = 0;
	effectbase();
	effectbase(String _sName, int32_t _pluginType, int32_t _projectGlobalId);
	virtual ~effectbase();
	SafeRef<effectbase> makeSafeRef();
	String getName() { return sName; };
	String getProductName() { return sProductName; };
	void setProductName(String sName) {
		this->sProductName = sName;
	#ifndef NDEBUG
		this->szName = this->sName.c_str();
	#endif
	}
	virtual int getModuleType() = 0;
	virtual void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) = 0;
	virtual guiplugin* makeGui() = 0;
	virtual guiplugin* getGui() = 0;
	virtual void process(AudioBlock* in, AudioBlock* out, int32_t samples) = 0;
	virtual void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed);
	virtual bool show() = 0;
	virtual bool close() = 0;
	virtual void resume() = 0;
	virtual void sleep() = 0;
	virtual void unload(vsthost* host) { assert(nLoadCalls==1); nLoadCalls--; };
	virtual void load(vsthost* host) { assert(nLoadCalls==0); nLoadCalls++; };
	virtual int32_t getDelay() = 0;
	virtual String getInfo(std::vector<String>& list) = 0;
	track_t* getTrack() override;
	virtual void onTick(double since);
	virtual void getChildAudioStages(std::vector<audio_stage_t*>& targets) {

	}
	virtual void loadSnapshot(const plugin_snapshot_t& snapshot) = 0;
	virtual void breakTrackLink();
	virtual void setTrackLink(audio_stage_t* audioStage);
	virtual void onPreUnload() {

	}
	virtual bool isBypass() {
		return !this->bIsEnabled;
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
protected:
	friend class effect_deferred;
	effect_deferred* toDeferred();
};
struct effect_deferred_impl;
class effect_deferred : public effectbase {
public:
	effect_deferred_impl* mImpl = nullptr;
public:
	//	deffered_effect();
	~effect_deferred();
	void loadSnapshot(const plugin_snapshot_t& snapshot) override;
	int32_t getDelay() override;
	String getInfo(std::vector<String>& list) override;
	int getModuleType() override;
	void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) override;
	guiplugin* makeGui() override;
	guiplugin* getGui() override;
	void process(AudioBlock* in, AudioBlock* out, int32_t samples) override;
	bool show() override;
	bool close() override;
	void resume() override;
	void sleep() override;
	String getAutomatableName() override;
	float getParamValue(int32_t idx) override;
	void setParamValue(int32_t idx, float val, int flags) override;
	automationlane_snapshot_t toRef() override;
	static std::shared_ptr<effect_deferred> fromEffect(effectbase* eff);
	String getDfrdPluginName();
	plugin_snapshot_t getSnapshot() const;
	void onPreUnload() override;
};
effect_deferred* loadPluginDeferred(const plugin_snapshot_t& snapshot);
//std::shared_ptr<effect_deferred> loadPluginDeferred(const plugin_snapshot_t& snapshot);
void removePlugin(effectbase* module);

