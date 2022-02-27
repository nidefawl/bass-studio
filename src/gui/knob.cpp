#include "knob.h"
#include "basectrl.h"
#include "knoblabeled.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "guicontextmenu.h"
#include "guicontextmenu_daw.h"
#include "theme.h"
#include "guitooltip.h"
#include "str_util.h"
#include "color_util.h"
#include "keyboard.h"
#include "table.h"
#include "logging.h"
#include "automation.h"
#include "host/mainctrl.h"

using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

template<>
void guitooltip<guiknob>::layout() {
    size.x          = 220;
    table.rowHeight = FONT_SIZE_TOOLTIP + INSET_TABLE_CELL_PADDING * 2;
    table.rows.clear();
    table.titleCols.clear();
    table.colSizes.clear();
    //row1.cols.push_back();
    using tbl_rows = std::vector<table_entry_t>;
    {
        tbl_rows vec{ tblstr{ "value" }, tblfloat{ ptr->getValue() } };
        table.rows.push_back(tbl_row_t{ vec });
    }
    Table::AdjustColSizes(table, getSizeContent() - ivec2(INSET_TABLE << 1));
    size.y = table.rows.size() * table.rowHeight;
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
    renderButtonAt(vg, insetP, insetS);
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
            setValue(value, FLG_PAR_UPDATE_USER);
            evt.dragDistance->y = 0;
            lastVal             = value;
            changedValue        = true;
        }
    }
}
void guiknob::handleDraggedRelease(MouseEvent& evt) {
    if (changedValue) {
        onValueEditFinish(fModifyBeginValue, lastVal);
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
void guiknob::renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS) {
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
    float minSize       = math::min(insetS.x, insetS.y);
    float r             = (minSize * 0.8f) / 2.0f;
    float lineThickness = math::max(1.0f, roundf((minSize / 8.0f) * 2.0f) / 2.0f);
    nvgLineCap(vg, NVGlineCap::NVG_ROUND);
    float val = getValueClamped();
    if (isSlider) {
        lineThickness = math::max(1.0f, roundf((minSize / 32.0f) * 2.0f) / 2.0f);
        float cx      = insetP.x;
        float cy      = insetP.y;
        float height  = insetS.y;
        nvgBeginPath(vg);
        nvgRect(vg, cx, cy, insetS.x, height);
        nvgFillColor(vg, G_WHITE);
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
        nvgStrokeColor(vg, G_WHITE);
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

void guiknob::setAutomationHandlers() {
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
        if (paramAutomatable) {
            MainCtrl::get()->showAutomation(paramAutomatable->getTrack(), paramAutomatable, paramIdx);
        }
    };
#endif
}


void guiknob_labeled_base::layout() {
    int buttonSize    = size.x * 0.6f;
    int left          = (size.y - buttonSize);
    float scaleTop    = 0.35f;
    float scaleBottom = 0.25f;
    labelHeight       = math::max(14.0f, left * scaleTop);
    valueHeight       = math::max(14.0f, left * scaleBottom);
    if (isSlider) {
        if (label.length() < 12) {
            labelHeight = math::max(14.0f, left * 0.15f);
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
    ivec2 insetP = pos + ivec2(button_inset, labelHeight);
    ivec2 insetS = size - ivec2(button_inset * 2, labelHeight + valueHeight);
    if (isSlider) {
        insetP = pos + ivec2(1, labelHeight);
        insetS = size - ivec2(2, labelHeight + valueHeight);
    }
    if (insetS.x < 0 || insetS.y < 0) {
        return;
    }
    const int INS_BRD = 2;
//    renderWidgetBorder(vg);
//    renderWidgetBorderPosSize(vg, getStateFlags(), pos + glm::ivec2(0, labelHeight + INS_BRD),
//                              size - glm::ivec2(0, labelHeight+valueHeight+INS_BRD*2));
    auto renderBorder = [this](NVGcontext* vg, int32_t flags, ivec2 pos, ivec2 size, GuiColor::constant_t bgColor) {
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgStrokeColor(vg, theme->getBgStrokeColor(flags));
        nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_GUI_FRAME_STROKE_WIDTH));
        nvgStroke(vg);
        nvgFillColor(vg, theme->getColor(bgColor));
        nvgFill(vg);
    };
    auto bgColor       = getBackgroundColor(getStateFlags());
    auto contrastColor = getContrastFontColor(nvgToRGB(bgColor));
    renderButtonAt(vg, insetP, insetS);
    if (labelHeight) {
        renderBorder(vg, getStateFlags(), pos + glm::ivec2(0, +INS_BRD), glm::ivec2(size.x, labelHeight - INS_BRD * 2), GuiColor::COL_BG_BRT);
    }
    renderBorder(vg, getStateFlags(), pos + glm::ivec2(0, size.y - valueHeight + INS_BRD), glm::ivec2(size.x, valueHeight - INS_BRD * 2), GuiColor::COL_BG_BRT);
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
//    setFont(vg, (int) ((knob->size.y / 2.0)), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

    UIFont::font_instance instance = theme->getFont(UIFont::FONT_DEFAULT);
    UIFont::bindFont(vg, instance);

    if (isSlider) {
        nvgFillColor(vg, contrastColor);
        if (labelHeight) {
            nvgFontSize(vg, (int32_t) G_FONT_SCALE(labelHeight * 0.5f));
            nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(labelHeight), StringAsCStr(label), NULL);
        }
        nvgFontSize(vg, (int32_t) G_FONT_SCALE(valueHeight * 0.5f));
        nvgText(vg, pos.x + size.x / 2.0f, pos.y + size.y - valueHeight + G_FONT_MIDDLE_OFFSET(valueHeight), StringAsCStr(valueDisplay),
                NULL);

    } else {
        nvgFillColor(vg, contrastColor);
        if (labelHeight) {
            nvgFontSize(vg, (int32_t) G_FONT_SCALE(labelHeight - 2.0f));
            nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(labelHeight), StringAsCStr(label), NULL);
        }
        nvgFontSize(vg, (int32_t) G_FONT_SCALE(valueHeight - 2.0f));
        nvgText(vg, pos.x + size.x / 2.0f, pos.y + size.y - valueHeight + G_FONT_MIDDLE_OFFSET(valueHeight), StringAsCStr(valueDisplay),
                NULL);
    }
}

void guiknob::handleRightClick(MouseEvent& evt) {
#if BUILD_VSTHOST
    if (paramAutomatable && paramIdx > -1) {
        dbgassert(dawCtrl);
        if (dawCtrl) {
            dbgassert(paramAutomatable->getParam(paramIdx));
            dawCtrl->openContextMenu(new guictxtmenu_at_param(paramAutomatable, paramIdx), evt.mousepos);
        }
        return;
    }
#endif
    if (parent)
        parent->rightClicked(evt, this);
}
