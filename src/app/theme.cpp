#include <nanovg.h>
#include <vector>
#include "theme.h"
#include "color_util.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "seq_math.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


uint32_t nvgToRGB(NVGcolor c);
uint32_t nvgToRGBA(NVGcolor c);
NVGcolor getContrastFontColor(uint32_t color);

namespace GuiColor {
std::vector<constant_t> getAllConstants();
}
namespace GuiConstant {
std::vector<constant_t> getAllConstants();
}

bool nvgColorEqual(NVGcolor a, NVGcolor b) {
#define F_EPS 0.000001f
	if (fabs(a.r-b.r) > F_EPS) return false;
	if (fabs(a.g-b.g) > F_EPS) return false;
	if (fabs(a.b-b.b) > F_EPS) return false;
	if (fabs(a.a-b.a) > F_EPS) return false;
	return true;
}

guitheme_t::guitheme_t() {
	initTheme();
}

void guitheme_t::initTheme() {
	vecNVGColors.resize(NUM_GUI_COLORS);
	mapColors.clear();
	mapProperties.clear();
	std::vector<GuiColor::constant_t> v = GuiColor::getAllConstants();
	for (auto c : v) {
		mapColors[c.idx] = c.defValue;
		this->vecNVGColors[c.idx] = rgbaToNvg(c.defValue);
	}
	std::vector<GuiConstant::constant_t> v2 = GuiConstant::getAllConstants();
	for (auto c : v2) {
		mapProperties[c.idx] = c.defValue;
	}
	uint32_t rgb = nvgToRGB(getColor(GuiColor::COL_BG_DRK));
	setTint(rgb);
}

NVGcolor& guitheme_t::getColorRef(GuiColor::constant_t _constant) {
	assert(_constant.idx >= 0 && _constant.idx < this->vecNVGColors.size());
#ifndef NDEBUG
	//Make sure the 2 are in sync
	auto it = mapColors.find(_constant.idx);
	if (it != mapColors.end()) {
		assert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(it->second)));
	}
#endif
	return this->vecNVGColors[_constant.idx];
}
NVGcolor guitheme_t::getColor(GuiColor::constant_t _constant) const {
	assert(_constant.idx >= 0 && _constant.idx < this->vecNVGColors.size());
#ifndef NDEBUG
	//Make sure the 2 are in sync
	auto it = mapColors.find(_constant.idx);
	if (it != mapColors.end()) {
		assert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(it->second)));
	}
#endif
	return this->vecNVGColors[_constant.idx];
}
NVGcolor guitheme_t::getContrastColor(GuiColor::constant_t _constant) const {
	assert(_constant.idx >= 0 && _constant.idx < this->vecNVGColors.size());
	return getContrastFontColor(nvgToRGBA(this->vecNVGColors[_constant.idx]));
}
int32_t guitheme_t::getColorInt32(GuiColor::constant_t _constant) {
    auto it = mapColors.find(_constant.idx);
    if (it == mapColors.end()) {
    	//Make sure the 2 are in sync
    	assert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(_constant.defValue)));
		return _constant.defValue;
    }
	//Make sure the 2 are in sync
	assert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(mapColors[_constant.idx])));
	return mapColors[_constant.idx];
}
void guitheme_t::setColor(GuiColor::constant_t _constant, int32_t _newValue) {
	if (isDefault)
		return;
	assert(_constant.idx >= 0 && _constant.idx < this->vecNVGColors.size());
	assert(_constant.idx < NUM_GUI_COLORS);
	mapColors[_constant.idx] = _newValue;
	this->vecNVGColors[_constant.idx] = rgbaToNvg(_newValue);
}

const float guitheme_t::getFloat(GuiConstant::constant_t _constant) {
	return get(_constant)/10.0f;
}
const int32_t guitheme_t::get(GuiConstant::constant_t _constant) {
    auto it = mapProperties.find(_constant.idx);
    if (it == mapProperties.end()) {
    	return _constant.defValue;
    }
    int32_t val = mapProperties[_constant.idx];
    assert(val >= 0 && val <= 10000);
	return mapProperties[_constant.idx];
}
void guitheme_t::set(GuiConstant::constant_t _constant, int32_t _value) {
	mapProperties[_constant.idx] = _value;
}
void guitheme_t::setTint(uint32_t hex) {
	glm::vec4 hsl = hexToHSL(hex);
	colorBg = nvgHSL(hsl.x, hsl.y, hsl.z);
	colorBgDisabled = nvgHSL(hsl.x, CLAMP_F(hsl.y*0.55f), CLAMP_F(hsl.z - 0.3f));
//	colorBgActive = nvgRGBAf(1, 0, 0, 1);
	colorBgStroke = nvgHSL(hsl.x, CLAMP_F(hsl.y*1.3f), 0.4f);
	colorBgFocused = nvgHSL(hsl.x, CLAMP_F(hsl.y*0.85f), CLAMP_F(hsl.z + 0.15f));
	colorBgHover = nvgHSL(hsl.x, CLAMP_F(hsl.y*0.7f), CLAMP_F(hsl.z - 0.3f));
	colorBgPressed = nvgHSL(hsl.x, CLAMP_F(hsl.y*1.15f), CLAMP_F(hsl.z - 0.1f));
//	colorBgPressed = colorBgPressed = colorBgFocused = colorBgHover;
	const int lvl = 18;
	colorBgFrameOutline = GUI_COLOR(lvl);
	colorBgFrameBase = GUI_COLOR(CMUL(lvl, 1.33));
	colorBgFrameHighlight = GUI_COLOR(CMUL(lvl, 1.66));
	colorBgFrameBright = GUI_COLOR(CMUL(lvl, 2.2));
}

const NVGcolor guitheme_t::getBgColor(int32_t flags) {
	if (!(flags & FLG_ENBL)) {
		return colorBgDisabled;
	}
	if (flags & FLG_DRG) {
		return colorBgPressed;
	}
//	if (flags & FLG_FOC) {
//		return colorBgFocused;
//	}
//	if (flags & FLG_HVRD) {
//		return colorBgHover;
//	}
	return colorBg;
}
const NVGcolor guitheme_t::getBgStrokeColor(int32_t flags) {
	if (!(flags & FLG_ENBL)) {
		return colorBgDisabled;
	}
	if (flags & FLG_FOC) {
		return colorBgFocused;
	}
	if (flags & FLG_HVRD) {
		return colorBgHover;
	}
	return colorBgStroke;
}
