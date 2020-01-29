#pragma once
#include "../vstsdk-host-2.4/aeffectx.h"
#include <memory>

#include "../../gui/guiplugin.h"

struct audio_stage_t;
class AudioEffectX;
struct handles_t {
	AudioEffectX* axEffect = NULL; // Optional/Internal plugin only: handle to plugin implementation instance
	AEffect* aeffect = NULL; // hmodule owns if axEffect == null
	void* hmodule = NULL; // we dont own
	std::unique_ptr<guiplugin> gui;
	handles_t(AudioEffectX* ex, AEffect* e, void* m) {
		axEffect = ex;
		aeffect = e;
		hmodule = m;
	}
	~handles_t();
};
