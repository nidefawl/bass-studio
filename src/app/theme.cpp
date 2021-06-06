#include <nanovg.h>
#include <vector>
#include "theme.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "color_util.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "renderresources.h"
#include "assert_dbg.h"


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


struct guitheme_t::guitheme_override_state_t {
	int32_t overrideState = 0;
	int32_t animationTime = 0;
	GuiColor::constant_t _overrideColorConstant;
	int32_t _overrideColorValue = 0;
	GuiConstant::constant_t _overrideConstantConstant;
	int32_t _overrideConstantValue= 0;
	void updateAnimation() {
		if (this->overrideState) {
			this->animationTime++;
			if (this->animationTime > 15) {
                this->overrideState ^= 1<<4;
				this->animationTime = 0;
				//this->overrideState = 0;
			}
		}
	}
	void pingConstant(GuiColor::constant_t _constant, int32_t colorContrastPing) {
		overrideState = 1;
		_overrideColorConstant = _constant;
        _overrideColorValue = colorContrastPing;

	}
	void endPing() {
		overrideState = 0;
	}
	void pingConstant(GuiConstant::constant_t _constant) {
		overrideState = 2;
		_overrideConstantConstant = _constant;
		_overrideConstantValue = 5;
	}

};
guitheme_t::guitheme_t() {
	this->overrideState = new guitheme_t::guitheme_override_state_t{};
	initTheme();
}

void guitheme_t::initTheme() {
	vecNVGColors.resize(NUM_GUI_COLORS);
	mapColors.clear();
	mapProperties.clear();
	mapFonts.clear();
	std::vector<GuiColor::constant_t> v = GuiColor::getAllConstants();
	for (auto c : v) {
		mapColors[c.idx] = c.defValue;
		this->vecNVGColors[c.idx] = rgbaToNvg(c.defValue);
	}
	std::vector<GuiConstant::constant_t> v2 = GuiConstant::getAllConstants();
	for (auto c : v2) {
		mapProperties[c.idx] = c.defValue;
	}
	auto v3 = UIFont::getAllConstants();
	for (auto c : v3) {
		mapFonts[c.idx] = UIFont::font_instance{c.defValue};
	}
}

NVGcolor& guitheme_t::getColorRef(GuiColor::constant_t _constant) {
	dbgassert(_constant.idx >= 0 && _constant.idx < this->vecNVGColors.size());
#ifndef NDEBUG
	//Make sure the 2 are in sync
	auto it = mapColors.find(_constant.idx);
	if (it != mapColors.end()) {
		dbgassert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(it->second)));
	}
#endif
	return this->vecNVGColors[_constant.idx];
}
NVGcolor guitheme_t::getColor(GuiColor::constant_t _constant) const {
    dbgassert(_constant.idx >= 0 && _constant.idx < this->vecNVGColors.size());
    if (overrideState->overrideState == 1 && overrideState->_overrideColorConstant.idx == _constant.idx) {
        return rgbaToNvg(this->overrideState->_overrideColorValue);
    }
#ifndef NDEBUG
	//Make sure the 2 are in sync
	auto it = mapColors.find(_constant.idx);
	if (it != mapColors.end()) {
		dbgassert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(it->second)));
	}
#endif
	return this->vecNVGColors[_constant.idx];
}
NVGcolor guitheme_t::getContrastColor(GuiColor::constant_t _constant) const {
	if (overrideState->overrideState == 1 && overrideState->_overrideColorConstant.idx == _constant.idx) {
		return getContrastFontColor(this->overrideState->_overrideColorValue);
	}
	dbgassert(_constant.idx >= 0 && _constant.idx < this->vecNVGColors.size());
	return getContrastFontColor(nvgToRGBA(this->vecNVGColors[_constant.idx]));
}
int32_t guitheme_t::getColorInt32(GuiColor::constant_t _constant) {
	if (overrideState->overrideState == 1 && overrideState->_overrideColorConstant.idx == _constant.idx) {
		return this->overrideState->_overrideColorValue;
	}
    auto it = mapColors.find(_constant.idx);
    if (it == mapColors.end()) {
    	//Make sure the 2 are in sync
    	dbgassert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(_constant.defValue)));
		return _constant.defValue;
    }
	//Make sure the 2 are in sync
	dbgassert(nvgColorEqual(this->vecNVGColors[_constant.idx], rgbaToNvg(mapColors[_constant.idx])));
	return mapColors[_constant.idx];
}
void guitheme_t::setColor(GuiColor::constant_t _constant, int32_t _newValue) {
	if (isDefault)
		return;
	dbgassert(_constant.idx >= 0 && _constant.idx < this->vecNVGColors.size());
	dbgassert(_constant.idx < NUM_GUI_COLORS);
	mapColors[_constant.idx] = _newValue;
	this->vecNVGColors[_constant.idx] = rgbaToNvg(_newValue);
}

float guitheme_t::getFloat(GuiConstant::constant_t _constant) {
	return get(_constant)/10.0f;
}
int32_t guitheme_t::get(GuiConstant::constant_t _constant) {
    auto it = mapProperties.find(_constant.idx);
    if (it == mapProperties.end()) {
    	return _constant.defValue;
    }
    int32_t val = mapProperties[_constant.idx];
    dbgassert(val >= 0 && val <= 10000);
	return mapProperties[_constant.idx];
}
UIFont::font_instance guitheme_t::getFont(UIFont::font_type_t _fonttype) const {
    auto it = mapFonts.find(_fonttype.idx);
    if (it == mapFonts.end()) {
    	return UIFont::font_instance{_fonttype.defValue};
    }
    return mapFonts.at(_fonttype.idx);
}
void guitheme_t::bindFonts() {
	for (auto it = mapFonts.begin(); it != mapFonts.end(); ++it) {
		int32_t key = it->first;
		UIFont::font_type_t c = UIFont::getConstantById(key);
		if (c.idx <= 0)
			continue;
		it->second.fontInstanceIdx = -1;
//		for (int i = 0; i < MAX_FONTS; i++) {
//			if (RenderResources::fontsLoaded[i].name == it->second.name) {
//				it->second.fontInstanceIdx = i;
//				return;
//			}
//		}
	}
}
UIFont::font_instance guitheme_t::setFont(UIFont::font_type_t _fonttype, String s) {
	mapFonts[_fonttype.idx] = UIFont::font_instance{s};
	return mapFonts[_fonttype.idx];
}
void guitheme_t::set(GuiConstant::constant_t _constant, int32_t _value) {
	mapProperties[_constant.idx] = _value;
}

NVGcolor guitheme_t::getBgColor(int32_t flags) const {
	if (!(flags & FLG_ENBL)) {
		return getColor(GuiColor::COL_BASE_BG_DISABLED);
	}
	if (flags & FLG_DRG) {
		return getColor(GuiColor::COL_BASE_BG_PRESSED);
	}
//	if (flags & FLG_FOC) {
//		return colorBgFocused;
//	}
//	if (flags & FLG_HVRD) {
//		return colorBgHover;
//	}
	return getColor(GuiColor::COL_BASE_BG);
}
NVGcolor guitheme_t::getBgStrokeColor(int32_t flags) const {
	if (flags & FLG_FOC) {
		return getColor(GuiColor::COL_BASE_BG_FOCUSED);
	}
	if (flags & FLG_HVRD) {
		return getColor(GuiColor::COL_BASE_BG_HOVER);
	}
//	if (!(flags & FLG_ENBL)) {
//		return colorBgDisabled;
//	}
	return getColor(GuiColor::COL_BASE_BG_STROKE);
}
NVGcolor guitheme_t::getFrameColorOutline() const {
	return getColor(GuiColor::COL_BASE_BG_FRAME_OUTLINE);
}
NVGcolor guitheme_t::getFrameColorBase() const {
	return getColor(GuiColor::COL_BASE_BG_FRAME_BASE);
}
NVGcolor guitheme_t::getFrameColorHighlight() const {
	return getColor(GuiColor::COL_BASE_BG_FRAME_HIGHLIGHT);
}
NVGcolor guitheme_t::getFrameColorBright() const {
	return getColor(GuiColor::COL_BASE_BG_FRAME_BRIGHT);
}

void guitheme_t::updateAnimation() {
	this->overrideState->updateAnimation();
}
void guitheme_t::pingConstant(GuiColor::constant_t _constant)
{
    int32_t colorContrastPing = 0;
    if (this->mapColors.count(_constant.idx)) {
        colorContrastPing = 0xFF000000|getContrastFontColoru32(this->mapColors.at(_constant.idx));
    }
    this->overrideState->pingConstant(_constant, colorContrastPing);

}
void guitheme_t::pingConstant(GuiConstant::constant_t _constant) {
	this->overrideState->pingConstant(_constant);
}
void guitheme_t::endPing() {
	this->overrideState->endPing();
}
