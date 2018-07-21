#pragma once
#include <stdint.h>
#include <vector>
#include "str_util.h"

struct param_snapshot_t {
	int32_t idx;
	float val;
};
struct automation_view_t;
struct plugin_snapshot_t {
	int32_t projectGlobalId;
	bool present;
	bool enabled;
	int32_t slot;
	int32_t pluginType;
	int32_t uId;
	String name;
	std::vector<uint8_t> dataChunk;
	std::vector<uint8_t> dataChunk2;
	std::vector<param_snapshot_t> params;
	std::vector<automation_view_t> automatedParams;
	std::vector<plugin_snapshot_t> pluginSnapshots;
};
