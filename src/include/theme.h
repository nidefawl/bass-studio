#pragma once
#include <vector>
#include <nanovg_min.h>
#include <unordered_map>
#include "guicolors.h"
#include "guiconstant.h"
#include "guifonts.h"
#include "str_util.h"

NVGcolor rgbaToNvg(uint32_t color);

struct guitheme_t {
	String name = "";
	String fileName = "";
	bool isDefault;
	NVGcolor colorBg{};
	NVGcolor colorBgStroke{};
	NVGcolor colorBgHover{};
	NVGcolor colorBgPressed{};
	NVGcolor colorBgFocused{};
	NVGcolor colorBgDisabled{};
	NVGcolor colorBgFrameBase{};
	NVGcolor colorBgFrameOutline{};
	NVGcolor colorBgFrameHighlight{};
	NVGcolor colorBgFrameBright{};
	std::vector<NVGcolor> vecNVGColors;
	std::unordered_map<int32_t, int32_t> mapColors;
	std::unordered_map<int32_t, int32_t> mapProperties;
	std::unordered_map<int32_t, UIFont::font_instance> mapFonts;
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
	const NVGcolor getFrameColorBase() const {
		return this->colorBgFrameBase;
	}
	const NVGcolor getFrameColorHighlight() const {
		return this->colorBgFrameHighlight;
	}
	const NVGcolor getFrameColorBright() const {
		return this->colorBgFrameBright;
	}
	NVGcolor& getColorRef(GuiColor::constant_t _constant);
	NVGcolor getColor(GuiColor::constant_t _constant) const;
	NVGcolor getContrastColor(GuiColor::constant_t _constant) const;
	int32_t getColorInt32(GuiColor::constant_t _constant);
	UIFont::font_instance getFont(UIFont::font_type_t _fonttype) const;
	void bindFonts();
	UIFont::font_instance setFont(UIFont::font_type_t _fonttype, String s);
	void setColor(GuiColor::constant_t _constant, int32_t _newValue);
	int32_t get(GuiConstant::constant_t _constant);
	float getFloat(GuiConstant::constant_t _constant);
	void set(GuiConstant::constant_t _constant, int32_t _newValue);
};
