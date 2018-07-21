#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include "internal_plugin.h"
#include "str_util.h"
#include "snapshot.h"

struct internal_handles_t;
struct audio_stage_t;
class guiplugin;
class guibase;
class vsthost;
class module_group : public internalplugin {
	internal_handles_t* handle;
	audio_stage_t* audio;
public:
	module_group(int32_t _projectGlobalId);
	~module_group();
	float dispatchGetParameter(int32_t idx) override;
	void dispatchSetParameter(int32_t idx, float val) override;
public:
	guiplugin* makeGui() override;
	guiplugin* getGui() override;
	int32_t getDelay() override;
	void process(AudioBlock* in, AudioBlock* out, int32_t samples) override;
	virtual String getInfo(std::vector<String>& list) override;
	bool resume() override;
	bool sleep() override;
	void unload() override;
	void load(vsthost* host) override;
	virtual void breakTrackLink() override;
	virtual void setTrackLink(audio_stage_t* trImpl) override;
	audio_stage_t* getAudioStage() { return audio; };
	void onTick(double since) override;
	void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) override;
	void loadSnapshot(const plugin_snapshot_t& snapshot) override;
};

