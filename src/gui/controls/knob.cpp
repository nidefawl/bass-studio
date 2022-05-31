#include "knob.h"
#include "basectrl.h"
#include "knoblabeled.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "math/seq_math.h"
#include "platform.h"
#include "theme.h"
#include "gui/tooltip/tooltip.h"
#include "str_util.h"
#include "color_util.h"
#include "keyboard.h"
#include "gui/table/table.h"
#include "logging.h"
#include "automation.h"
#include "host/mainctrl.h"
#if BUILD_VSTHOST
#include "gui/track/trackcontent.h"
#endif

using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

template<>
void guitooltip<guiknob>::setContent() {
    table.tableWidth = 80;
    table.rows.push_back({ { tblfloat{ ptr->getValue() } } });
}

guictxtmenu_base* guiknob::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<guiknob>(this);
    return tooltip;
}

bool guiknob::isAutomated() {
    //#if BUILD_VSTHOST
    if (paramAutomatable) {
        auto at = paramAutomatable->getRegisteredAutomation(paramIdx);
        return at && at->isAutomated();
    }
    //#endif
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
    ivec2 insetP = pos + ivec2(0);
    ivec2 insetS = size - ivec2(0);
    renderButtonAt(vg, insetP, insetS, getValue());
}
void guiknob::handleDraggedBegin(MouseEvent& evt) {
    if (isShift(evt.kbmods) || (bDoubleClickSetsDefault && evt.type == MouseEventType::M_EVT_DOUBLECLICK)) {
        setToDefaultValue();
        return;
    }
    if (evt.guiDragged == this) {
        parentCtrl->captureMouse(this);
    }
    fModifyBeginValue = lastVal = getValue();
    changedValue                = false;
}
void guiknob::handleDraggedMove(MouseEvent& evt) {
    if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
        int disty = (int) evt.dragDistance->y;
        if (math::abs(disty) < 1)
            return;
        float value = lastVal;
        float scale = isCtrl(evt.kbmods) ? 2000.0f : 200.0f;
        float delta = disty / scale;
        if (math::abs(delta) > 1e-2f) {
            value -= delta;
            if (!changedValue && fnValueEditBegin) {
                fnValueEditBegin(lastVal, value);
            }
            setValue(value, FLG_PAR_UPDATE_USER);
            evt.dragDistance->y = 0;
            lastVal             = value;
            changedValue        = true;
        }
    }
}
void guiknob::handleDraggedRelease(MouseEvent& evt) {
    if (changedValue && fnValueEditFinish) {
        fnValueEditFinish(fModifyBeginValue, lastVal);
    }
    changedValue = false;
}
bool guiknob::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    float value = getValue();
    float scale = isCtrl(evt.kbmods) ? 200.0f : 20.0f;
    value += yoffset / scale;
    setValue(value, FLG_PAR_UPDATE_USER);
    return true;
}
void guiknob::renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS, float value) {
    renderWidgetBorder(vg, getStateFlags());

    NVGcolor c2 = theme->getColor(GuiColor::COL_BG_BRT);
    if (hovered())
        c2 = theme->getColor(GuiColor::COL_BG_DRKER);
    if (focused())
        c2 = theme->getColor(GuiColor::COL_BG_DRKER2);
    if ((hovered() || focused())) {
//        nvgBeginPath(vg);
//        nvgCircle(vg, cx, cy, r * 1.5f);
//        nvgFillColor(vg, c2);
//        nvgFill(vg);
    }
    float val = CLAMP_F(value);
    float minSize       = math::min(insetS.x, insetS.y);
    float r             = (minSize * 0.8f) / 2.0f;
    float lineThickness = math::max(1.0f, roundf((minSize / 8.0f) * 2.0f) / 2.0f);
    nvgLineCap(vg, NVGlineCap::NVG_ROUND);
    if (isSlider) {
        lineThickness = math::max(1.0f, roundf((minSize / 32.0f) * 2.0f) / 2.0f);
        float cx      = insetP.x;
        float cy      = insetP.y;
        float height  = insetS.y;
        nvgBeginPath(vg);
        nvgRect(vg, cx, cy, insetS.x, height);
        nvgFillColor(vg, THEMECOL_TEXT);
        nvgFill(vg);
        float heightRange = insetS.y * val;
        nvgBeginPath(vg);
        nvgRect(vg, cx, cy + height - heightRange, insetS.x, heightRange);
        nvgFillColor(vg, theme->getColor(valColor));
        nvgFill(vg);
        float heightHandle = math::max(3.0f, lineThickness + 3.0f);
        nvgBeginPath(vg);
        nvgRect(vg, cx, cy + height - heightRange - heightHandle * 0.5f, insetS.x, heightHandle);
        c2.a = 0.5f;
        nvgFillColor(vg, c2);
        nvgFill(vg);
    } else {

        float cx = insetP.x + insetS.x / 2.0f;
        float cy = insetP.y + insetS.y / 1.8f;
        vec2 center(cx, cy);
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, r, start, start + range, NVG_CW);
        nvgStrokeColor(vg, THEMECOL_TEXT);
        nvgStrokeWidth(vg, lineThickness);
        nvgStroke(vg);
        float end = start + val * range;
        if (val > 1E-8F) {
            nvgBeginPath(vg);
            nvgArc(vg, cx, cy, r, start, end, NVG_CW);
            nvgStrokeColor(vg, theme->getColor(valColor));
            nvgStrokeWidth(vg, lineThickness + 1.0f);
            nvgStroke(vg);
        }

        nvgBeginPath(vg);
        nvgCircleFast(vg, cx, cy, r * 0.7f);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircleFast(vg, cx, cy, r * 0.7f - 1.5f);
        nvgFillColor(vg, c2);
        nvgFill(vg);
        vec2 pos(cosf(end), sinf(end));
        vec2 posStart = pos * 1.5f + center;
        vec2 posEnd   = pos * r * 0.7f + center;
        nvgBeginPath(vg);
        nvgMoveTo(vg, posStart.x, posStart.y);
        nvgLineTo(vg, posEnd.x, posEnd.y);
        nvgStrokeColor(vg, theme->getColor(indColor));
        nvgStrokeWidth(vg, math::max(1.0f, roundf((r / 8.0f) * 2.0f) / 2.0f));
        nvgStroke(vg);
        nvgLineCap(vg, NVGlineCap::NVG_BUTT);
    }
}

void guiknob::setToDefaultValue() {
    setValue(fDefaultValue, FLG_PAR_UPDATE_USER);
}

void guiknob::setKnobInternalHandlers() {
#if BUILD_VSTHOST
    fnGetValue = [this]() {
        if (paramAutomatable) {
            return paramAutomatable->getParamValue(paramIdx);
        }
        return value;
    };
    fnSetValue = [this](float f, int flags) {
        if (paramAutomatable) {
            ThreadLock lock     = MainCtrl::getPlayThread()->lockThread();
            automation_t* param = paramAutomatable->getRegisteredAutomation(paramIdx);
            if (param) {
                param->active = false;
            }
            paramAutomatable->setParamValue(paramIdx, f, flags);
        }
    };
    fnValueEditFinish = [this](float preVal, float val) {
        if (paramAutomatable) {
            paramAutomatable->postSetParameter(paramIdx, preVal, val, FLG_PAR_UPDATE_USER);
        }
    };
    fnFocus = [this](MouseHitEvt& evt, bool focused) {
        if (paramAutomatable && dawCtrl) {
            auto* track = paramAutomatable->getTrack();
            if (!track)
                return;
            auto* guiTrackCtr   = dawCtrl->getTrackContainer();
            track_gui_entry_t* entry{};
            if (!guiTrackCtr->getPointerEntry(track, &entry))
                return;
            guiTrackCtr->showAutomationLane(entry, paramAutomatable, paramIdx);
            dawCtrl->updateVisibleTrackContents();
            guiTrackCtr->scrollTo(entry->content);
        }
    };
#endif
}


void guiknob_labeled_base::layout() {
    // auto buttonSize   = math::roundfS32(size.x * 0.8f);
    // auto left         = (size.y - buttonSize);
    float scaleTop    = 0.12f;
    float scaleBottom = 0.12f;
    labelHeight       = math::roundfS32(math::max(14.0f, size.y * scaleTop));
    valueHeight       = math::roundfS32(math::max(14.0f, size.y * scaleBottom));
    if (isSlider) {
        if (label.length() < 12) {
            labelHeight = math::roundfS32(math::max(14.0f, size.y * 0.15f));
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
    const int INS_BRD = 6;
    ivec2 pLabel = pos + ivec2(INS_BRD);
    ivec2 pValue = pos + ivec2(INS_BRD, size.y-(INS_BRD+valueHeight));
    ivec2 sLabel = ivec2(size.x - 2 * INS_BRD, labelHeight);
    ivec2 sValue = ivec2(size.x - 2 * INS_BRD, valueHeight);
    ivec2 pKnob = pos + ivec2(INS_BRD, INS_BRD+labelHeight);
    ivec2 sKnob = size - ivec2(0, labelHeight+valueHeight + 2 * INS_BRD);

    // ivec2 insetS = size - ivec2(INS_BRD * 2);
    // if (isSlider) {
    //     insetP = pos + ivec2(1, labelHeight);
    //     insetS = size - ivec2(2, labelHeight + valueHeight);
    // }
    auto renderBorder = [this](NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size, GuiColor::constant_t bgColor) {
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgStrokeColor(vg, theme->getBgStrokeColor(flags));
        nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
        nvgStroke(vg);
        nvgFillColor(vg, theme->getColor(bgColor));
        nvgFill(vg);
    };
    float value = getValue();

    if (fnGetDisplayValue) {
        valueDisplay = fnGetDisplayValue(value);
    }
    if (sKnob.x > 0 && sKnob.y > 0) {
        renderButtonAt(vg, pKnob, sKnob, value);
    }
    if (sLabel.x > 0 && sLabel.y > 0) {
        renderBorder(vg, getStateFlags(), pLabel, sLabel, GuiColor::COL_BG_BRT);
    }
    if (sValue.x > 0 && sValue.y > 0) {
        renderBorder(vg, getStateFlags(), pValue, sValue, GuiColor::COL_BG_BRT);
    }
    auto bgColor       = theme->getColor(getBackgroundColor());
    auto contrastColor = getContrastFontColor(nvgToRGB(bgColor));
    nvgFillColor(vg, contrastColor);
    if (sLabel.x > 0 && sLabel.y > 0) {
        renderTextLabel(vg, vec2(pLabel) + vec2(sLabel) * 0.5f, sLabel, label, theme, labelHeight, contrastColor, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
    if (sValue.x > 0 && sValue.y > 0) {
        renderTextLabel(vg, vec2(pValue) + vec2(sValue) * 0.5f, sValue, valueDisplay, theme, valueHeight, contrastColor, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    }
}

void guiknob::handleRightClick(MouseEvent& evt) {
#if BUILD_VSTHOST
    if (paramAutomatable && paramIdx > -1) {
        dbgassert(dawCtrl);
        if (dawCtrl) {
            dbgassert(paramAutomatable->getParam(paramIdx));
            dawCtrl->openContextMenu(new guictxtmenu_at_param(dawCtrl, paramAutomatable, paramIdx), evt.mousepos);
        }
        return;
    }
#endif
    if (parent)
        parent->rightClicked(evt, this);
}
