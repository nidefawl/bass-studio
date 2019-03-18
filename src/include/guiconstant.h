#pragma once
#include <stdint.h>
#include <nanovg_min.h>
#include "str_util.h"
#include <vector>

namespace GuiConstant {
//int32_t getNextId();
struct constant_t {
	int32_t idx;
	const char* name;
	int32_t defValue;
	constant_t();
	constant_t(const char* _name, int32_t _defValue);
};
std::vector<constant_t> getAllConstants();
constant_t getConstantById(int32_t id);
constant_t getConstantByName(String name);
extern constant_t CONST_PLUGIN_TITLE_HEIGHT;
extern constant_t CONST_TRACK_HEIGHT_STEP;
extern constant_t CONST_TRACK_HEIGHT_TITLE;
extern constant_t CONST_METER_WIDTH;
//constant_t CONST_PLUGIN_TITLE_HEIGHT("CONST_PLUGIN_TITLE_HEIGHT", 24+INSET_TRACK_CONTENT*2);
//constant_t CONST_TRACK_HEIGHT_STEP("CONST_TRACK_HEIGHT_STEP", 24+INSET_TRACK_CONTENT*2);
//	constant_t CONST_METER_WIDTH("CONST_METER_WIDTH", 32);
}
