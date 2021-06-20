#pragma once
#include "../vstsdk-host-2.4/aeffectx.h"
#include <memory>

#include "../../gui/guiplugin.h"

struct audio_stage_t;
class AudioEffectX;
class PluginViewContainers;
struct handles_t {
	int32_t localCurrentUniqueId = 0;
	VstTimeInfo localTimeInfo{0};
	AudioEffectX* axEffect = NULL; // Optional/Internal plugin only: handle to plugin implementation instance
	AEffect* aeffect = NULL; // hmodule owns if axEffect == null
	void* hmodule = NULL; // we dont own
	std::unique_ptr<guiplugin> gui;
	std::vector<uint8_t> dataChunkLocalMemory;
	handles_t(AudioEffectX* ex, AEffect* e, void* m) {
		axEffect = ex;
		aeffect = e;
		hmodule = m;
	}
	~handles_t();
};
