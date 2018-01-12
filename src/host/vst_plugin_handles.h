#pragma once
#include "../vst_sdk_2.4/aeffectx.h"
#include <minwindef.h>
#include <memory>
#include "../gui/plugin.h"

struct handles_t {
	AEffect* aeffect = NULL; // hmodule owns
	HMODULE hmodule = NULL; // we dont own
	track_impl_t* tr_plugins = NULL;
	std::unique_ptr<guiplugin> gui;
	int32_t slot = -1;
	handles_t(AEffect* e, HMODULE m) {
		aeffect = e;
		hmodule = m;
	}
	~handles_t();
};
