#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "knob.h"
#include "knoblabeled.h"
#include "gui.h"
#include "guitooltip.h"
#include "str_util.h"
#include "table.h"
#include "logging.h"
#include "automation.h"
using glm::ivec2;
using glm::vec2;
using namespace Table;
namespace GuiColor {
constant_t COL_KNOB("COL_KNOB", 0xff00ddff);
constant_t COL_KNOB_IND("COL_KNOB_IND", 0xffffffff);
constant_t COL_AUTOMATED("COL_KNOB_AUT", 0xFFEF62DF);
}

template <>
void guitooltip<guiknob>::layout()  {
	size.x = 220;
	table.rowHeight = FONT_SIZE_TOOLTIP+INSET_TABLE_CELL_PADDING*2;
	table.rows.clear();
	table.titleCols.clear();
	table.colSizes.clear();
//	row1.cols.push_back();
	using tbl_rows = std::vector<table_entry_t>;
	{
		tbl_rows vec{tblstr{"value"}, tblfloat{ptr->getValue()}};
		table.rows.push_back(tbl_row_t{vec});
	}
	Table::AdjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* guiknob::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<guiknob>(this); //why does casting m_clip to (clip_t*) break the ptr?
	return tooltip;
//	appctrl->openContextMenu(tooltip, appctrl->m_mousePos);
}

bool guiknob::isAutomated() {
#ifdef BUILD_BUILTIN_EFFECT
	if (paramAutomatable) {
		auto at = paramAutomatable->getRegisteredAutomation(paramIdx);
		return at && at->src.isAutomated();
	}
#endif
	return false;
}
void guiknob::render(NVGcontext* vg) {
	if (isAutomated()) {
		valColor = GuiColor::COL_AUTOMATED;
		indColor = GuiColor::COL_AUTOMATED;
	} else {
		indColor = GuiColor::COL_KNOB_IND;
		valColor = GuiColor::COL_KNOB;
	}
	ivec2 insetP = pos+ivec2(0);
	ivec2 insetS = size-ivec2(0);
	renderButtonAt(vg, insetP, insetS);
}
bool guiknob::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
	if (contains(mpos)) {
		if (evt.type == MouseHitType::MOUSE_LEFT) {
			my_printf("click knob %s\n", StringAsCStr(label));
		}
		evt.requestFocus(this);
		return true;
	}
	return false;
}
void guiknob::renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS) {
	if (renderBackground) {
		renderWidgetBorder(vg);
//			nvgBeginPath(vg);
//			nvgRect(vg, insetP.x, insetP.y, insetS.x, insetS.y);
//			nvgFillColor(vg, GUI_COLORRGB(200, 50, 200, 180));
//			nvgFill(vg);
	}

	NVGcolor c2 = theme->getColor(GuiColor::COL_BG_BRT);
	if (hovered())
		c2 = theme->getColor(GuiColor::COL_BG_DRKER);
	if (focused())
		c2 = theme->getColor(GuiColor::COL_BG_DRKER2);
	if ((hovered() || focused())) {
//		    nvgBeginPath(vg);
//		    nvgCircle(vg, cx, cy, r*1.5f);
//		    nvgFillColor(vg, c2);
//			nvgFill(vg);
	}
    float minSize = min(insetS.x, insetS.y);
    float r = (minSize*0.8f)/2.0f;
    float lineThickness = max(1.0f, roundf((minSize / 8.0f)*2.0f)/2.0f);
	nvgLineCap(vg, NVGlineCap::NVG_ROUND);
	float val = getValueClamped();
	if (isSlider) {
		lineThickness = max(1.0f, roundf((minSize / 32.0f)*2.0f)/2.0f);
	    float cx = insetP.x;
	    float cy = insetP.y;
	    float height = insetS.y;
	    nvgBeginPath(vg);
	    nvgRect(vg, cx, cy, insetS.x, height);
	    nvgFillColor(vg, G_WHITE);
		nvgFill(vg);
	    float heightRange = insetS.y*val;
	    nvgBeginPath(vg);
	    nvgRect(vg, cx, cy+height-heightRange, insetS.x, heightRange);
	    nvgFillColor(vg, theme->getColor(valColor));
		nvgFill(vg);
	    float heightHandle = std::max(3.0f, lineThickness+3.0f);
	    nvgBeginPath(vg);
	    nvgRect(vg, cx, cy+height-heightRange-heightHandle*0.5f, insetS.x, heightHandle);
	    c2.a = 0.5f;
	    nvgFillColor(vg, c2);
		nvgFill(vg);
	} else {

	    float cx = insetP.x+insetS.x/2.0f;
	    float cy = insetP.y+insetS.y/1.8f;
		vec2 center(cx, cy);
	    nvgBeginPath(vg);
	    nvgArc(vg, cx, cy, r, start, start+range, NVG_CW);
		nvgStrokeColor(vg, G_WHITE);
		nvgStrokeWidth(vg, lineThickness);
		nvgStroke(vg);
		float end = start + val * range;
	    if (val > 1E-8F) {
		    nvgBeginPath(vg);
		    nvgArc(vg, cx, cy, r, start, end, NVG_CW);
			nvgStrokeColor(vg, theme->getColor(valColor));
			nvgStrokeWidth(vg, lineThickness+1.0f);
			nvgStroke(vg);
	    }

	    nvgBeginPath(vg);
	    nvgCircleFast(vg, cx, cy, r*0.7f);
	    nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
		nvgFill(vg);
	    nvgBeginPath(vg);
	    nvgCircleFast(vg, cx, cy, r*0.7f-1.5f);
	    nvgFillColor(vg, c2);
		nvgFill(vg);
		vec2 pos(cosf(end), sinf(end));
		vec2 posStart = pos*1.5f+center;
		vec2 posEnd = pos*r*0.7f+center;
		nvgBeginPath(vg);
		nvgMoveTo(vg, posStart.x, posStart.y);
		nvgLineTo(vg, posEnd.x, posEnd.y);
		nvgStrokeColor(vg, theme->getColor(indColor));
		nvgStrokeWidth(vg, max(1.0f, roundf((r/8.0f)*2.0f)/2.0f));
		nvgStroke(vg);
		nvgLineCap(vg, NVGlineCap::NVG_BUTT);
	}



}
#ifndef BUILD_NO_VST
#ifdef BUILD_BUILTIN_EFFECT
void guiknob::setAutomationHandlers() {
	my_printf("set handlers %012X\n", (int64_t)paramAutomatable);
	fnGetValue = [this] () {
		if (paramAutomatable) {
			return paramAutomatable->getParamValue(paramIdx);
		}
		return value;
	};
	fnSetValue = [this] (float f, int flags) {
		if (paramAutomatable) {
			automation_t* param = paramAutomatable->getAutomation(paramIdx);
			if (param) {
				param->active = false;
			}
			paramAutomatable->setParamValue(paramIdx, f, flags);
		}
	};
	fnValueEditFinish = [this](float preVal, float val) {
		if (paramAutomatable) {
			paramAutomatable->postSetParameter(paramIdx, preVal, val, 2);
		}
	};
}

#endif
#endif


void guiknob_labeled_base::layout() {
	int buttonSize = size.x * 0.6f;
	int left = (size.y - buttonSize);
	float scaleTop = 0.35f;
	float scaleBottom = 0.25f;
	labelHeight = std::max(14.0f, left * scaleTop);
	valueHeight = std::max(14.0f, left * scaleBottom);
	if (isSlider) {
		if (label.length() < 12) {
			labelHeight = std::max(14.0f, left * 0.15f);
		} else {
			labelHeight = 0;
		}
	}
}

void guiknob_labeled_base::render(NVGcontext* vg) {
	if (isAutomated()) {
		valColor = GuiColor::COL_AUTOMATED;
		indColor = GuiColor::COL_AUTOMATED;
	} else {
		indColor = GuiColor::COL_KNOB_IND;
		valColor = GuiColor::COL_KNOB;
	}
	//		nvgBeginPath(vg);
	//		nvgRect(vg, pos.x, pos.y, size.x, size.y);
	//		nvgFillColor(vg, GUI_COLORRGB(150, 150, 200, 180));
	//		nvgFill(vg);
	ivec2 insetP = pos + ivec2(button_inset, labelHeight);
	ivec2 insetS = size - ivec2(button_inset * 2, labelHeight + valueHeight);
	if (isSlider) {
		insetP = pos + ivec2(1, labelHeight);
		insetS = size- ivec2(2, labelHeight + valueHeight);
	}
	if (insetS.x < 0 || insetS.y < 0) {
		return;
	}
	const int INS_BRD = 2;
	//		renderWidgetBorder(vg);
	//		renderWidgetBorderPosSize(vg, getStateFlags(), pos + glm::ivec2(0, labelHeight+INS_BRD),
	//				size - glm::ivec2(0, labelHeight+valueHeight+INS_BRD*2));
	auto renderBorder = [this](NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size, GuiColor::constant_t bgColor) {
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgStrokeColor(vg, theme->getBgStrokeColor(flags));
		nvgStrokeWidth(vg, theme->getBgStrokeWidth(flags));
		nvgStroke(vg);
		nvgFillColor(vg, theme->getColor(bgColor));
		nvgFill(vg);
	};
	auto bgColor = theme->getBgColor(getStateFlags());
	auto contrastColor = getContrastFontColor(nvgToRGB(bgColor));
	renderButtonAt(vg, insetP, insetS);
	if (labelHeight) {
		renderBorder(vg, getStateFlags(), pos + glm::ivec2(0, +INS_BRD), glm::ivec2(size.x, labelHeight - INS_BRD * 2), GuiColor::COL_BG_BRT);
	}
	renderBorder(vg, getStateFlags(), pos + glm::ivec2(0, size.y - valueHeight + INS_BRD), glm::ivec2(size.x, valueHeight - INS_BRD * 2), GuiColor::COL_BG_BRT);
	nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
	if (isSlider) {
		nvgFillColor(vg, contrastColor);
		if (labelHeight) {
			nvgFontSize(vg, (int32_t) G_FONT_SCALE(labelHeight * 0.5));
			nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(labelHeight), StringAsCStr(label), NULL);
		}
		nvgFontSize(vg, (int32_t) G_FONT_SCALE(valueHeight * 0.5));
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + size.y - valueHeight + G_FONT_MIDDLE_OFFSET(valueHeight), StringAsCStr(valueDisplay),
				NULL);

	} else {
		nvgFillColor(vg, contrastColor);
		if (labelHeight) {
			nvgFontSize(vg, (int32_t) G_FONT_SCALE(labelHeight - 2));
			nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(labelHeight), StringAsCStr(label), NULL);
		}
		nvgFontSize(vg, (int32_t) G_FONT_SCALE(valueHeight - 2));
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + size.y - valueHeight + G_FONT_MIDDLE_OFFSET(valueHeight), StringAsCStr(valueDisplay),
				NULL);

	}
}
