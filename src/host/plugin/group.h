#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include "modules.h"
#include "internal_plugin.h"
#include "str_util.h"
#include "snapshot.h"

struct audio_stage_t;
class guiplugin;
class guibase;
class vsthost;
class module_group : public internalplugin {
	struct internal_handles_t;
	internal_handles_t* handle;
	audio_stage_t* audio;
public:
	module_group(int32_t _projectGlobalId);
	~module_group();
	float dispatchGetParameter(int32_t idx) override;
	void dispatchSetParameter(int32_t idx, float val) override;
public:
	virtual int getModuleType() override { return PLUGIN_TYPE_GROUP; };
	guiplugin* makeGui() override;
	guiplugin* getGui() override;
	int32_t getDelay() override;
	void process(AudioBlock* in, AudioBlock* out, int32_t samples) override;
	String getInfo(std::vector<String>& list) override;
	void resume() override;
	void sleep() override;
	void unload(vsthost* host) override;
	void onPreUnload() override;
	void load(vsthost* host) override;
	void breakTrackLink() override;
	void setTrackLink(audio_stage_t* trImpl) override;
	audio_stage_t* getAudioStage() { return audio; };
	void onTick(double since) override;
	void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) override;
	void loadSnapshot(const plugin_snapshot_t& snapshot) override;
	void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
	void getChildAudioStages(std::vector<audio_stage_t*>& targets) override;
};

