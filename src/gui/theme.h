#pragma once
#include <nanovg.h>
#include "guicolors.h"
#include "color_util.h"

struct guitheme_t {
	const bool isDefault;
	NVGcolor colorBg;
	NVGcolor colorBgStroke;
	NVGcolor colorBgHover;
	NVGcolor colorBgPressed;
	NVGcolor colorBgFocused;
	guitheme_t(bool _isDefault) : isDefault(_isDefault) {
		initDefaultTheme();
	}
	void initDefaultTheme() {

		uint32_t rgb = nvgToRGB(g_guiColors[COL_BG_DRK]);
		setBgColor(rgb);
		colorBgStroke = g_guiColors[COL_GUI_STROKE];
	}
	void setBgColor(uint32_t hex) {
		vec4 hsl = hexToHSL(hex);
		colorBg = nvgHSL(hsl.x, hsl.y, hsl.z);
		colorBgStroke = nvgHSL(hsl.x, CLAMP_F(hsl.y*1.3f), 0.4f);
		colorBgFocused = nvgHSL(hsl.x, CLAMP_F(hsl.y*0.85f), CLAMP_F(hsl.z + 0.15f));
		colorBgHover = nvgHSL(hsl.x, CLAMP_F(hsl.y*0.7f), CLAMP_F(hsl.z + 0.3f));
		colorBgPressed = nvgHSL(hsl.x, CLAMP_F(hsl.y*1.15f), CLAMP_F(hsl.z - 0.1f));
	}
	const NVGcolor getBgStrokeColor(int32_t flags) {
		if (!(flags & FLG_ENBL)) {
			if (flags & FLG_FOC) {
				return G_WHITE;
			}
			if (flags & FLG_HVRD) {
				return G_WHITE;
			}
		}
		return colorBgStroke;
	}
	const NVGcolor getBgColor(int32_t flags) {
		if (!(flags & FLG_ENBL)) {
			return G_BUTTON_DISABLED;
		}
		if (flags & FLG_DRG) {
			return colorBgPressed;
		}
		if (flags & FLG_HVRD) {
			return colorBgHover;
		}
		if (flags & FLG_FOC) {
			return colorBgFocused;
		}
		return colorBg;
	}
	float getBgStrokeWidth(int32_t flags) {
		return G_STROKE;
	}
};
