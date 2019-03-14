#pragma once
#include <nanovg.h>
#include "guicolors.h"
#include "seq_math.h"
#include "color_util.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <unordered_map>

struct guitheme_t {

	const bool isDefault;
	NVGcolor colorBg;
	NVGcolor colorBgStroke;
	NVGcolor colorBgHover;
	NVGcolor colorBgPressed;
	NVGcolor colorBgFocused;
	NVGcolor colorBgDisabled;
	NVGcolor colorBgFrameBase;
	NVGcolor colorBgFrameOutline;
	NVGcolor colorBgFrameHighlight;
	NVGcolor colorBgFrameBright;
	NVGcolor guiColors[NUM_GUI_COLORS];
	std::unordered_map<int32_t, int32_t> values;

	guitheme_t(bool _isDefault) : isDefault(_isDefault) {
		initDefaultTheme();
	}
	void initDefaultTheme();
	void setBgColor(uint32_t hex);
	void setActiveColor(uint32_t hex) {
		glm::vec4 hsl = hexToHSL(hex);
		colorBg = nvgHSL(hsl.x, hsl.y, hsl.z);
	}
	const NVGcolor getBgColor(int32_t flags);
	const NVGcolor getBgStrokeColor(int32_t flags);
	float getBgStrokeWidth(int32_t flags) {
		return G_STROKE;
	}
	const NVGcolor getFrameColorOutline() {
		return this->colorBgFrameOutline;
	}
	const NVGcolor getFrameColorBase() {
		return this->colorBgFrameBase;
	}
	const NVGcolor getFrameColorHighlight() {
		return this->colorBgFrameHighlight;
	}
	const NVGcolor getFrameColorBright() {
		return this->colorBgFrameBright;
	}
	const NVGcolor getColor(int color);
	const int32_t get(int32_t _constant);
	void set(int32_t _constant, int32_t _newValue);
};
