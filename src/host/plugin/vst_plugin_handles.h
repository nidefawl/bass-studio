#pragma once
#include "../vst_sdk_2.4/aeffectx.h"
#include <memory>
#include "../../gui/plugin.h"

struct audio_stage_t;
struct handles_t {
	AEffect* aeffect = NULL; // hmodule owns
	void* hmodule = NULL; // we dont own
	audio_stage_t* tr_plugins = NULL;
	std::unique_ptr<guiplugin> gui;
	int32_t slot = -1;
	handles_t(AEffect* e, void* m) {
		aeffect = e;
		hmodule = m;
	}
	~handles_t();
};
