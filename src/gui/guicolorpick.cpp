#include "guicolorpick.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "glheaders.h"
#include <nanovg.h>
#include <vector>
#include <memory>
#include <stdint.h>
#include <nanovg.h>

#include "str_util.h"

#include "gui/knob.h"
#include "gui/button.h"
#include "gui/guicontainer.h"
#include "gui/textfield.h"
#include "guicontextmenu_daw.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
glm::vec4 rgbToHSL(float r, float g, float b) {
    float minV = std::min(std::min(r, g), b);
    float maxV = std::max(std::max(r, g), b);
    float h, s, l = (maxV + minV) / 2;

    if(maxV == minV){
        h = s = 0; // achromatic
    }else{
    	auto greatest = [](auto x, auto y, auto z){
    	  return x > y ? (x > z ? 0 : 2) : (y > z ? 1 : 2);
    	};
    	float d = maxV - minV;
        s = l > 0.5 ? d / (2 - maxV - minV) : d / (maxV + minV);
        int mx = greatest(r, g, b);
        if (mx == 0) {
        	h = (g - b) / d + (g < b ? 6 : 0);
        } else if (mx == 1) {
        	h = (b - r) / d + 2;
        } else {
        	h = (r - g) / d + 4;
        }
        h /= 6;
    }

	return glm::vec4{ h, s, l, 1.0f };

}

void gui_color_pick::buttonClicked(guibase* button) {
	if (button == &hexInput) {
		setInt32(colorInt32);
	}
}
void gui_color_pick::setInt32(int32_t rgba) {
	glm::vec4 color = colorHex(rgba);
	glm::vec4 hsla = rgbToHSL(color.x, color.y, color.z);
	this->knH.setValueInit(hsla.x);
	this->knS.setValueInit(hsla.y);
	this->knL.setValueInit(hsla.z);
	this->knA.setValueInit(color.w);
	this->colorInt32 = rgba;
	this->nvgColor = rgbaToNvg(rgba);
	if (ptrColorInt32) {
		*ptrColorInt32 = colorInt32;
	}
	if (ptrNvgColor) {
		*ptrNvgColor = nvgColor;
	}
	if (fnSetValue) {
		fnSetValue(rgba);
	}
}
void gui_color_pick::setHSL(float h, float s, float l, float a) {
	this->knH.setValueInit(h);
	this->knS.setValueInit(s);
	this->knL.setValueInit(l);
	this->knA.setValueInit(a);
	auto col = nvgHSL(h, s, l);
	int32_t rgb = nvgToRGB(col) & 0xFFFFFF;
	int32_t alpha = CLAMP_I((int32_t)(255.0f*a), 0, 255)<<24;
	int32_t rgba = rgb | alpha;
	this->colorInt32 = rgba;
	this->nvgColor = rgbaToNvg(rgba);
	if (ptrColorInt32) {
		*ptrColorInt32 = colorInt32;
	}
	if (ptrNvgColor) {
		*ptrNvgColor = nvgColor;
	}
	if (fnSetValue) {
		fnSetValue(rgba);
	}
}
void gui_color_pick::setHSL_(float h, float s, float l, float a) {
	auto col = nvgHSL(h, s, l);
	int32_t rgb = nvgToRGB(col) & 0xFFFFFF;
int32_t alpha = CLAMP_I((int32_t)(255.0f*a), 0, 255)<<24;
//	setInt32(rgb | alpha);
	int32_t rgba = rgb | alpha;
	this->colorInt32 = rgba;
	this->nvgColor = rgbaToNvg(rgba);
	if (ptrColorInt32) {
		*ptrColorInt32 = colorInt32;
	}
	if (ptrNvgColor) {
		*ptrNvgColor = nvgColor;
	}
	if (fnSetValue) {
		fnSetValue(rgba);
	}
}
void gui_color_pick::setRefInt32(int32_t* ptrInt32) {
	ptrColorInt32 = ptrInt32;
}
void gui_color_pick::setRefNvg(NVGcolor* ptrNvg) {
	ptrNvgColor = ptrNvg;
}
void gui_color_pick::layout() {
	int sizeQuad = size.y;
	float sliderW = size.y/4;
	for (auto* g : guis)
		g->size = vec2(sliderW, size.y);
	this->hexInput.pos = {0, sizeQuad/4*3};
	this->hexInput.size = {sizeQuad, sizeQuad/4};
	knA.pos = vec2(size.x-sliderW, 0);
	knL.pos = vec2(knA.left()-sliderW, 0);
	knS.pos = vec2(knL.left()-sliderW, 0);
	knH.pos = vec2(knS.left()-sliderW, 0);
	for (auto* g : guis)
		g->layout();
}
void gui_color_pick::init() {
	this->hexInput.setAlignCenter(true);
	guiknob_labeled_base* knobs[4] = {
		&knH, &knS, &knL, &knA
	};
	const char* knoblabels[4] = {
		"Hue", "Saturation", "Brightness", "Alpha"
	};
	auto setColor = [this]() {
		float h = knH.getValueClamped();
		float s = knS.getValueClamped();
		float v = knL.getValueClamped();
		float a = knA.getValueClamped();
		setHSL_(h, s, v, a);
	};
	auto setEditColor = [setColor](float preVal, float val) {
		setColor();
	};
	auto getDisplayValue = [](float val) {
		int32_t v = CLAMP_I((int32_t)(100.0f*val), 0, 100);
		return StringFormat("%d%%", v);
	};
	for (int i = 0; i < 4; i++) {
		auto* knob = knobs[i];
		knob->setIsSlider(true);
		knob->setLabel(knoblabels[i]);
		knob->setRenderBackground(true);
		knob->fnValueEditChanged = setEditColor;
		knob->fnValueEditFinish = setEditColor;
		knob->fnGetDisplayValue = getDisplayValue;
	}
	knA.fnGetDisplayValue = [](float val) {
		int32_t alpha = CLAMP_I((int32_t)(255.0f*val), 0, 255);
		return StringFormat("%d", alpha);
	};
}

void gui_color_pick::handleRightClick(MouseEvent& evt) {
	parentCtrl->openContextMenu(new guictxtmenu_colorpalette(), evt.mousepos);
}
