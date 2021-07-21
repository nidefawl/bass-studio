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
class PluginViewContainers;
struct track_impl_t;

class internalplugin : public effectbase {
protected:
	struct internalplugin_handles_t;
	internalplugin_handles_t* handlesIntPlugin;
public:
	std::vector<std::shared_ptr<PluginViewContainers>> views;
	String sDir;
	bool bInEditIdle = false;
	int pluginCategory = 0;
	int vstVersion = 0;
	int uId = 0;
	internalplugin(String _sName, int32_t _pluginType, int32_t _projectGlobalId);
	virtual ~internalplugin();
protected:
	virtual void onEnable();
	virtual void onDisable();
	virtual float dispatchGetParameter(int32_t idx) = 0;
	virtual void dispatchSetParameter(int32_t idx, float val) = 0;
public:
	virtual guiplugin* makeGui() override;
	virtual guiplugin* getGui() override;
//	virtual PluginViewContainers* createInternalView() = 0;
	virtual std::shared_ptr<PluginViewContainers> createInternalView() {
		return nullptr;
	};
	virtual int32_t getPluginLatency() = 0;
	virtual void process(AudioBlock* in, AudioBlock* out, double tick, int32_t samplePos, int32_t numSamples, playback_state state) = 0;
//	virtual bool resume() = 0;
//	virtual bool sleep() = 0;
//	virtual void unload(vsthost* host, int flags) = 0;
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
	automationlane_snapshot_t toRef() const override;

//	virtual std::shared_ptr<PluginViewContainers> createView() = 0;
};
