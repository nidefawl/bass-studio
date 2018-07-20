#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include "internal_plugin.h"
#include "str_util.h"

class guiplugin;
class vsthost;
struct internal_handles_t;
class module_empty : public internalplugin {
	internal_handles_t* handle;
public:
	module_empty(int32_t _projectGlobalId);
	~module_empty();
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
};

