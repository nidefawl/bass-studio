#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <vector>
#include "internal_plugin.h"
#include "str_util.h"

struct internal_handles_t;
class module_group : public internalplugin {
	internal_handles_t* handle;
public:
	module_group(int32_t _projectGlobalId);
	~module_group();
	float dispatchGetParameter(int32_t idx) override;
	void dispatchSetParameter(int32_t idx, float val) override;
public:
	guibase* makeGui() override;
	guibase* getGui() override;
	int32_t getDelay() override;
	void process(AudioBlock* in, AudioBlock* out, int32_t samples) override;
	virtual String getInfo(std::vector<String>& list) override;
};

