#pragma once
#include <vector>
#include <nanovg_min.h>
#include <unordered_map>
#include "guicolors.h"
#include "guiconstant.h"
#include "str_util.h"

NVGcolor rgbaToNvg(uint32_t color);

struct guitheme_t {
	String name = "";
	String fileName = "";
	bool isDefault;
	NVGcolor colorBg{0};
	NVGcolor colorBgStroke{0};
	NVGcolor colorBgHover{0};
	NVGcolor colorBgPressed{0};
	NVGcolor colorBgFocused{0};
	NVGcolor colorBgDisabled{0};
	NVGcolor colorBgFrameBase{0};
	NVGcolor colorBgFrameOutline{0};
	NVGcolor colorBgFrameHighlight{0};
	NVGcolor colorBgFrameBright{0};
	std::vector<NVGcolor> vecNVGColors;
	std::unordered_map<int32_t, int32_t> mapColors;
	std::unordered_map<int32_t, int32_t> mapProperties;
	guitheme_t();
    guitheme_t(const guitheme_t &) = default;
    ~guitheme_t() = default;
    guitheme_t(guitheme_t &&) noexcept = default;
    guitheme_t & operator= (const guitheme_t &) = default;
	guitheme_t & operator= (guitheme_t &&) noexcept = default;

	void initTheme();
	void setTint(uint32_t hex);
	void setBackgroundColor(uint32_t rgbaint32) {
		colorBg = rgbaToNvg(rgbaint32);
	}
	const NVGcolor getBgColor(int32_t flags);
	const NVGcolor getBgStrokeColor(int32_t flags);
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
	NVGcolor& getColorRef(GuiColor::constant_t _constant);
	NVGcolor getColor(GuiColor::constant_t _constant) const;
	NVGcolor getContrastColor(GuiColor::constant_t _constant) const;
	int32_t getColorInt32(GuiColor::constant_t _constant);
	void setColor(GuiColor::constant_t _constant, int32_t _newValue);
	const int32_t get(GuiConstant::constant_t _constant);
	const float getFloat(GuiConstant::constant_t _constant);
	void set(GuiConstant::constant_t _constant, int32_t _newValue);
};
