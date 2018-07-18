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
#include "base_plugin.h"

struct AudioBlock;
struct handles_t;
class track_t;
class guibase;
struct track_impl_t;

struct internalplugin_param {
	int32_t idx;
	float value;
	String shortLabel;
	String label;
};
class internalplugin : public effectbase {
public:
#ifndef NDEBUG
	//helper indicator in gdb.
	//gdb cannot display std::string when built without clib-debug flag (SLOW)
	const char* szName = NULL;
#endif
	String sName;
	String sDir;
	bool bEditOpen = false;
	bool bInEditIdle = false;
	int pluginCategory = 0;
	bool isSynth = false;
	int vstVersion = 0;
	int uId = 0;
	std::vector<internalplugin_param> params;
	std::vector<automated_param_t> automatedParams;
	int32_t slot = -1;
	track_impl_t* trackImpl = nullptr;
	internalplugin(int32_t _projectGlobalId) : effectbase(_projectGlobalId) {
	}
	virtual ~internalplugin() {
	}
protected:
	virtual float dispatchGetParameter(int32_t idx) = 0;
	virtual void dispatchSetParameter(int32_t idx, float val) = 0;
public:
	virtual guibase* makeGui() = 0;
	virtual guibase* getGui() = 0;
	virtual int32_t getDelay() = 0;
	virtual void process(AudioBlock* in, AudioBlock* out, int32_t samples) = 0;

	void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) override;
	void setSlot(int32_t i) override;
	int32_t getSlot() override;
	void breakTrackLink() override;
	void setTrackLink(track_impl_t* trImpl) override;
	track_impl_t* getTrackLink() override;
	bool show() override;
	bool close() override;
	bool hasParam(int32_t idx) override;
	int32_t getNumParameters() override;
	String getParamName(int32_t paramIdx) override;
	String getAutomatableName() override;
	float getParamValue(int32_t idx);
	void setParamValue(int32_t idx, float val);
	void recvPluginEditParamUpdate(int32_t idx);
	void updateAutomatedParameters(tick_t pos) override;
	automation_t* getAutomation(int32_t paramIdx) override;
	void deactivateAutomation(int32_t paramIdx) override;
	void getAutomated(std::vector<int32_t>& targets) override;
	automationlane_snapshot_t toRef() override;
	automated_param_t* getRegisteredAutomation(int32_t idx) override;
};
effectbase* makeModuleInstance(int32_t uid);
