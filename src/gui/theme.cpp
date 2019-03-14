#include <nanovg.h>
#include "theme.h"
#include "color_util.h"
#include "guicolors.h"
#include "seq_math.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>



void initColorArr(NVGcolor* g_guiColors, int colorVal);
extern int colorVal;
void guitheme_t::initDefaultTheme() {
	initColorArr(this->guiColors, colorVal);
	uint32_t rgb = nvgToRGB(this->guiColors[COL_BG_DRK]);
	setBgColor(rgb);
	colorBgStroke = this->guiColors[COL_GUI_STROKE];
}
const NVGcolor guitheme_t::getColor(int color) {
	if (color < 0 || color >= NUM_GUI_COLORS)
		color = 0;
	return this->guiColors[color];
}
const int32_t getDefaultVal(int32_t _constant) {
	switch (_constant) {
	case G_PLUGIN_TITLE_HEIGHT:
		return 24;
	case G_TRACK_HEIGHT_STEP:
	case G_HEIGHT_TRACK_TITLE:
		return 24+INSET_TRACK_CONTENT*2;
	default:
		break;
	}
	return 0;
}
const int32_t guitheme_t::get(int32_t _constant) {
    auto it = values.find(_constant);
    if (it == values.end()) {
    	values[_constant] = getDefaultVal(_constant);
    }
	return values[_constant];
}
void guitheme_t::set(int32_t _constant, int32_t _value) {
	values[_constant] = _value;
}
void guitheme_t::setBgColor(uint32_t hex) {
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
