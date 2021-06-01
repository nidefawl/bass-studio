#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <vector>

#include "str_util.h"

#include "modules.h"
#include "base_plugin.h"
#include "internal_plugin.h"
#include "../host/vst_host.h"
#include "track.h"
#include "track_impl.h"

class guiplugin;
class vsthost;
struct audio_stage_t;
//
//class module_gain : public internalplugin {
//	struct internal_handles_t;
//	internal_handles_t* handle;
//public:
//	module_gain(int32_t _projectGlobalId);
//	~module_gain();
//	float dispatchGetParameter(int32_t idx) override
//	{
//		return 0;
//	}
//	void dispatchSetParameter(int32_t idx, float val) override
//	{
//	}
//public:
//	int getModuleType() override { return PLUGIN_TYPE_GAIN; };
//	guiplugin* makeGui() override;
//	guiplugin* getGui() override;
//	int32_t getDelay() override
//	{
//		return 0;
//	}
//	void process(AudioBlock* in, AudioBlock* out, int32_t samplePos, int32_t numSamples, playback_state state) override
//	{
//		dbgassert(
//				getTrackLink()->sampleFormat == this->format && in->samples == format.blockSize && out->samples == format.blockSize
//						&& format.blockSize > 0 && format.sampleRate > 0);
//		out->copyFrom(in);
//	}
//	virtual String getInfo(std::vector<String>& list) override
//	{
//		return "";
//	}
//	void resume() override
//	{
//	}
//	void sleep() override
//	{
//	}
//	void unload(vsthost* host) override
//	{
//		effectbase::unload(host);
//	}
//	void load(vsthost* host) override
//	{
//		effectbase::load(host);
//		this->blockInputs = new AudioBlock(math::max(2, 2), format.blockSize);
//		this->blockOutputs = new AudioBlock(math::max(2, 2), format.blockSize);
//		bIsEnabled = this->getParamValue(PARAM_ENABLE) > 0.5;
//		if (bIsEnabled) {
//			this->resume();
//		} else {
//			this->sleep();
//		}
//	}
//	void breakTrackLink() override {
//		bIsSetup = false;
//		internalplugin::breakTrackLink();
//	}
//	void setTrackLink(audio_stage_t* trImpl) override {
//		bIsSetup = !!(trImpl);
//		internalplugin::setTrackLink(trImpl);
//	}
//	virtual bool isBypass() override {
//		return true;
//	}
//};
class module_gain : public internalplugin {
	struct internal_handles_t;
	internal_handles_t* handle;
public:
	module_gain(int32_t _projectGlobalId);
	~module_gain();
	float dispatchGetParameter(int32_t idx) override;
	void dispatchSetParameter(int32_t idx, float val) override;
public:
	virtual int getModuleType() override { return PLUGIN_TYPE_GAIN; };
	int32_t getDelay() override;
	void process(AudioBlock* in, AudioBlock* out, int32_t samplePos, int32_t numSamples, playback_state state) override;
	String getInfo(std::vector<String>& list) override;
	void resume() override;
	void sleep() override;
	void unload(vsthost* host) override;
	void onPreUnload() override;
	void load(vsthost* host) override;
	void breakTrackLink() override;
	void setTrackLink(audio_stage_t* trImpl) override;
	void onTick(double since) override;
	void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) override;
	void loadSnapshot(const plugin_snapshot_t& snapshot) override;
	void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
	virtual std::shared_ptr<PluginViewContainers> createInternalView() override;
    virtual String formatDisplayValue(int32_t idx) override;
};
