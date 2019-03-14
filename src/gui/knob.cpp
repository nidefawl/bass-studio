#include "knob.h"
#include "gui.h"
#include "guitooltip.h"
#include "str_util.h"
#include "table.h"
#include "logging.h"
#include "automation.h"


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
	adjustColSizes(table, getSizeContent()-ivec2(INSET_TABLE<<1));
	size.y = table.rows.size()*table.rowHeight;
}

guictxtmenu_base* guiknob::getTooltip(AppCtrl* appctrl) {
	auto tooltip = new guitooltip<guiknob>(this); //why does casting m_clip to (clip_t*) break the ptr?
	return tooltip;
//	appctrl->openContextMenu(tooltip, appctrl->m_mousePos);
}

void guiknob::updateAutomationState(NVGcontext* vg) {
#ifdef BUILD_BUILTIN_EFFECT
	if (paramAutomatable) {
		auto at = paramAutomatable->getRegisteredAutomation(paramIdx);
		if (at && at->src.isAutomated()) {
			indColor = G_PURPLE;
		} else {
			indColor = G_WHITE;
		}
		if (at && at->src.isActive()) {
			valColor = G_PURPLE;
		} else {
			valColor = G_BLUE;
		}
	}
#endif
}
void guiknob::render(NVGcontext* vg) {
	updateAutomationState(vg);
	ivec2 insetP = pos+ivec2(0);
	ivec2 insetS = size-ivec2(0);
	renderButtonAt(vg, insetP, insetS);
}
void guiknob::renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS) {
	if (renderBackground) {
		renderWidgetBorder(vg);
//			nvgBeginPath(vg);
//			nvgRect(vg, insetP.x, insetP.y, insetS.x, insetS.y);
//			nvgFillColor(vg, GUI_COLORRGB(200, 50, 200, 180));
//			nvgFill(vg);
	}
	nvgLineCap(vg, NVGlineCap::NVG_ROUND);
    float cx = insetP.x+insetS.x/2.0f;
    float cy = insetP.y+insetS.y/1.8f;
    	    vec2 center(cx, cy);
    float minSize = min(insetS.x, insetS.y);
//	    float r = (minSize*0.66f)/2.0f;
    float r = (minSize*0.8f)/2.0f;
    float lineThickness = max(1.0f, roundf((minSize / 8.0f)*2.0f)/2.0f);

	NVGcolor c2 = theme->getColor(COL_BG_BRT);
	if (hovered())
		c2 = theme->getColor(COL_BG_DRKER);
	if (focused())
		c2 = theme->getColor(COL_BG_DRKER2);
	if ((hovered() || focused())) {
//		    nvgBeginPath(vg);
//		    nvgCircle(vg, cx, cy, r*1.5f);
//		    nvgFillColor(vg, c2);
//			nvgFill(vg);
	}
//	    nvgBeginPath(vg);
//	    vec2 pts[3] = {
//				vec2(cosf(start), sinf(start)),
//				vec2(0, -1),
//				vec2(cosf(start+range), sinf(start+range)),
//	    };
//		for (vec2 v : pts) {
//			vec2 vStart = v * (r-lineThickness*0.5f) + center;
//			vec2 vEnd = v * (r+lineThickness*1.2f) + center;
//		    nvgMoveTo(vg, vStart.x, vStart.y);
//		    nvgLineTo(vg, vEnd.x, vEnd.y);
//	    }
//		nvgStrokeColor(vg, theme->getColor(COL_GRID_BRT));
//		nvgStrokeWidth(vg, max(1.0f, round((r/16.0f)*2.0f)/2.0f));
//		nvgStroke(vg);

    nvgBeginPath(vg);
    nvgArc(vg, cx, cy, r, start, start+range, NVG_CW);
	nvgStrokeColor(vg, G_WHITE);
	nvgStrokeWidth(vg, lineThickness);
	nvgStroke(vg);
	float val = getValue();
	float end = start + val * range;
    if (val > 1E-8F) {
	    nvgBeginPath(vg);
	    nvgArc(vg, cx, cy, r, start, end, NVG_CW);
		nvgStrokeColor(vg, valColor);
		nvgStrokeWidth(vg, lineThickness+1.0f);
		nvgStroke(vg);
    }

    nvgBeginPath(vg);
    nvgCircleFast(vg, cx, cy, r*0.7f);
    nvgFillColor(vg, theme->getColor(COL_BG_DRKER2));
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
	nvgStrokeColor(vg, indColor);
	nvgStrokeWidth(vg, max(1.0f, roundf((r/8.0f)*2.0f)/2.0f));
	nvgStroke(vg);
	nvgLineCap(vg, NVGlineCap::NVG_BUTT);


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
