#pragma once
#include "../vst_sdk_2.4/aeffectx.h"
#include <windows.h>
#include <memory>
#include "../gui/plugin.h"

struct handles_t {
	AEffect* aeffect = NULL; // hmodule owns
	HMODULE hmodule = NULL; // we dont own
	track_plugins_t* tr_plugins = NULL;
	std::unique_ptr<guiplugin> gui;
	int32_t slot = -1;
	handles_t(AEffect* e, HMODULE m) {
		aeffect = e;
		hmodule = m;
	}
	~handles_t();
};
