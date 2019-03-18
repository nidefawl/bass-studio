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
class guiplugin;
struct track_impl_t;

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
	internalplugin(int32_t _pluginType, int32_t _projectGlobalId) : effectbase(_pluginType, _projectGlobalId) {
	}
	virtual ~internalplugin() {
	}
protected:
	virtual void onEnable();
	virtual void onDisable();
	virtual float dispatchGetParameter(int32_t idx) = 0;
	virtual void dispatchSetParameter(int32_t idx, float val) = 0;
public:
	virtual guiplugin* makeGui() = 0;
	virtual guiplugin* getGui() = 0;
	virtual int32_t getDelay() = 0;
	virtual void process(AudioBlock* in, AudioBlock* out, int32_t samples) = 0;
//	virtual bool resume() = 0;
//	virtual bool sleep() = 0;
//	virtual void unload(vsthost* host) = 0;
//	virtual void load(vsthost* host) = 0;

	virtual void loadSnapshot(const plugin_snapshot_t& snapshot) override;
	virtual void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) override;
	void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
	bool show() override;
	bool close() override;

	//automatble_t
	String getAutomatableName() override;
	float getParamValue(int32_t idx) override;
	void setParamValue(int32_t idx, float val, int flags) override;
	void recvPluginEditParamUpdate(int32_t idx);
	automationlane_snapshot_t toRef() override;
};
effectbase* makeModuleInstance(int32_t moduleType, int32_t moduleId, int32_t globalid);
