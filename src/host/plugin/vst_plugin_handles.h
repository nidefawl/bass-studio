#pragma once
#include "../vst_sdk_2.4/aeffectx.h"
#include <memory>
#include "../../gui/plugin.h"

struct audio_stage_t;
struct handles_t {
	AEffect* aeffect = NULL; // hmodule owns
	void* hmodule = NULL; // we dont own
	std::unique_ptr<guiplugin> gui;
	handles_t(AEffect* e, void* m) {
		aeffect = e;
		hmodule = m;
	}
	~handles_t();
};
