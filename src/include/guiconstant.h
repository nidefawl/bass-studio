#pragma once
#include <stdint.h>
#include <nanovg_min.h>
#include "str_util.h"
#include <vector>

namespace GuiConstant {

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
extern constant_t CONST_FIXED_TITLE_HEIGHT;
extern constant_t CONST_GUI_FRAME_STROKE_WIDTH;

}
