#include "knob.h"
#include "assert_dbg.h"
#include "basectrl.h"
#include "knoblabeled.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "math/seq_math.h"
#include "math/vec.h"
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
#include "threads/threadlock.h"
#include <cstdint>
#include <nanovg.h>
#include <nanovg_min.h>
#if BUILD_DAW_HOST
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
    if (paramAutomatable) {
        return paramAutomatable->getActiveAutomation(paramIdx);
    }
    return false;
}
bool guiknob::isModulated() {
    if (paramAutomatable) {
        return DAW::IsParamModulated(paramAutomatable, paramIdx);
    }
    return false;
}

void guiknob::setColors() {
    if (isHighlighted()) {
        valColor = GuiColor::COL_KNOB_HIGHLIGHT;
        indColor = GuiColor::COL_KNOB_HIGHLIGHT;
    } else if (isModulated()) {
        valColor = GuiColor::COL_KNOB_MODULATED;
        indColor = GuiColor::COL_KNOB_MODULATED;
    } else if (isAutomated()) {
        valColor = GuiColor::COL_AUTOMATED;
        indColor = GuiColor::COL_AUTOMATED;
    } else {
        indColor = GuiColor::COL_KNOB_IND;
        valColor = GuiColor::COL_KNOB;
    }
}

void guiknob::render(NVGcontext* vg) {
    ivec2 insetP = pos + ivec2(0);
    ivec2 insetS = size - ivec2(0);
    renderButtonAt(vg, insetP, insetS, getValue());
}
void guiknob::handleDraggedBegin(MouseEvent& evt) {
    if (isCtrl(evt.kbmods)) {
        parent->buttonClicked(this);
        return;
    }
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
        float scaleCtrlFine = isCtrl(evt.kbmods) ? 20.0f : 1.0f;
        float scaleGlobal = 400.0f;
        float delta = disty / (scaleCtrlFine * scaleGlobal);
        if (math::abs(delta) > 1e-12f && math::abs(delta) > getQuantizationStep()) {
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
    float q = getQuantizationStep();
    if (q > 0.0f) {
        value += q * (yoffset > 0 ? 1 : -1);
    } else {
        value += yoffset / scale;
    }
    setValue(value, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
    return true;
}
void guiknob::renderRangeIndicator(NVGcontext* vg, ivec2 insetP, ivec2 insetS, float rangeValueMin, float rangeValueMax, NVGcolor color, int idx, int numRanges) {
    float knobValue = math::clamp(getValue(), bIsBipolar ? -1.0f : 0.0f, 1.0f);
    float cx      = insetP.x;
    float cy      = insetP.y;
    float width   = insetS.x;
    float height  = insetS.y;
    if (knobType == knobtype::SLIDER_LABELED) {
        auto posModulation = height - height * knobValue;
        auto heightModulation = height * (rangeValueMax - rangeValueMin);
        posModulation -= height * rangeValueMax;
        vec4 r = {
            0,
            posModulation,
            static_cast<float>(insetS.x),
            heightModulation
        };
        // clip rect to 0, 0, width, height
        if (r.w < 0) {
            r.y += r.w;
            r.w = -r.w;
        }
        if (r.y < 0) {
            r.w += r.y;
            r.y = 0;
        }
        if (r.x < 0) {
            r.z += r.x;
            r.x = 0;
        }
        if (r.y + r.w > height) {
            r.w = height - r.y;
        }
        if (r.x + r.z > width) {
            r.z = width - r.x;
        }
        if (r.z > 0.5f && r.w > 0.5f) {
            float wIndicator = math::max(2.0f, r.z/numRanges);
            float xSlot = r.x + r.z - wIndicator;
            float wSlot = wIndicator;
            nvgBeginPath(vg);
            nvgRect(vg, cx + xSlot, cy + r.y, wSlot, r.w);
            dbgassert(r.x >= 0 && r.y >= 0 && r.z <= insetS.x && r.w <= insetS.y);
            nvgFillColor(vg, color);
            nvgFill(vg);
        }
    } else {
        // float radius             = (minSize * 0.8f) / 2.0f;
        // float lineThickness = math::max(1.0f, roundf((minSize / 8.0f) * 2.0f) / 2.0f);
    }

}
void guiknob::renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS, float value) {
    setColors();
    renderWidgetBorder(vg, getStateFlags());

    NVGcolor c2 = theme->getColor(GuiColor::COL_BG_BRT);
    if (hovered())
        c2 = theme->getColor(GuiColor::COL_BG_DRKER);
    if (focused())
        c2 = theme->getColor(GuiColor::COL_BG_DRKER2);

    float val     = math::clamp(value, bIsBipolar ? -1.0f : 0.0f, 1.0f);
    float minSize = math::min(insetS.x, insetS.y);
    float cx     = insetP.x;
    float cy     = insetP.y;
    float width  = insetS.x;
    float height = insetS.y;
    
    if (isBackgroundRendered()) {
        nvgBeginPath(vg);
        nvgRect(vg, cx, cy, insetS.x, height);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_KNOB_BG));
        nvgFillCustomPar(vg, -2);
        nvgFill(vg);
    }
    if (knobType == knobtype::SLIDER_LABELED) {
        float lineThickness = math::max(1.0f, roundf((minSize / 32.0f) * 2.0f) / 2.0f);
        float heightRange = insetS.y * val;
        nvgBeginPath(vg);
        nvgRect(vg, cx, cy + height - heightRange, insetS.x, heightRange);
        nvgFillColor(vg, theme->getColor(valColor));
        nvgFillCustomPar(vg, -3);
        nvgFill(vg);
        auto modRangesOptional = getKnobModulationRanges();
        if (modRangesOptional) {
            dbgassert(CtrSize(*modRangesOptional));
            const auto numMods = CtrSize(*modRangesOptional);
            for (int i = 0; i < numMods; i++) {
                auto& param = (*modRangesOptional)[i];
                auto posModulation = height - height * static_cast<float>(val);
                NVGcolor color = dbgcolorsArray[1 + (param.sourceId % (dbgcolorsArraySize-1))];
                color.a = 0.5f;
                auto& p = param.range;
                auto heightModulation = height*static_cast<float>(p)*(param.isBiPolar?2.0f:1.0f);
                if (param.isBiPolar) {
                    posModulation -= heightModulation * 0.5f;
                } else {
                    posModulation -= heightModulation;
                }
                vec4 r = {
                    0,
                    posModulation,
                    static_cast<float>(insetS.x),
                    heightModulation
                };
                // clip rect to 0, 0, width, height
                if (r.w < 0) {
                    r.y += r.w;
                    r.w = -r.w;
                }
                if (r.y < 0) {
                    r.w += r.y;
                    r.y = 0;
                }
                if (r.x < 0) {
                    r.z += r.x;
                    r.x = 0;
                }
                if (r.y + r.w > height) {
                    r.w = height - r.y;
                }
                if (r.x + r.z > width) {
                    r.z = width - r.x;
                }
                if (fabs(r.z) > 0.2f && fabs(r.w) > 0.2f) {
                    float wSlot = math::clamp((r.z*9/10) / numMods, 2.0f, r.z*0.33f);
                    float xSlot = r.x + wSlot * i;
                    while (wSlot > 4) {
                        float step = wSlot / 4.0f;
                        xSlot += step;
                        wSlot -= step * 2;
                        break;
                    }
                    nvgBeginPath(vg);
                    nvgRect(vg, cx + xSlot, cy + r.y, wSlot, r.w);
                    dbgassert(r.x >= 0 && r.y >= 0 && r.z <= insetS.x && r.w <= insetS.y);
                    nvgFillColor(vg, color);
                    nvgFill(vg);
                }
            }
        }
        float heightHandle = math::max(3.0f, lineThickness + 3.0f);
        nvgBeginPath(vg);
        nvgRect(vg, cx, cy + height - heightRange - heightHandle * 0.5f, insetS.x, heightHandle);
        c2.a = 0.5f;
        nvgFillColor(vg, c2);
        nvgFill(vg);
    } else {
        nvgLineCap(vg, NVGlineCap::NVG_ROUND);
        float lineThickness = math::max(1.0f, roundf((minSize / 8.0f) * 2.0f) / 2.0f);
        float radius        = (minSize * 0.8f) / 2.0f;
        cx = insetP.x + insetS.x / 2.0f;
        cy = insetP.y + insetS.y / 1.8f;
        vec2 center(cx, cy);
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, radius, start, start + range, NVG_CW);
        nvgStrokeColor(vg, THEMECOL_TEXT);
        nvgStrokeWidth(vg, lineThickness);
        nvgFillCustomPar(vg, -3);
        nvgStrokeCustomPar(vg, -3);
        nvgStroke(vg);
        float rangeScaled = bIsBipolar ? range * 0.5f : range;
        float startOffset = bIsBipolar ? start + rangeScaled : start;
        float end = startOffset + val * rangeScaled;
        if (fabs(val) > 1E-8F) {
            float endArc = end;
            if (endArc < startOffset) {
                std::swap(startOffset, endArc);
            }
            nvgBeginPath(vg);
            nvgArc(vg, cx, cy, radius, startOffset, endArc, NVG_CW);
            nvgStrokeColor(vg, theme->getColor(valColor));
            nvgStrokeWidth(vg, lineThickness + 1.0f);
            nvgFillCustomPar(vg, -3);
            nvgStrokeCustomPar(vg, -3);
            nvgStroke(vg);
        }

        nvgBeginPath(vg);
        nvgCircleFast(vg, cx, cy, radius * 0.7f);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
        nvgFill(vg);
        nvgBeginPath(vg);
        nvgCircleFast(vg, cx, cy, radius * 0.7f - 1.5f);
        nvgFillColor(vg, c2);
        nvgFill(vg);
        vec2 pos(cosf(end), sinf(end));
        vec2 posStart = pos * 1.5f + center;
        vec2 posEnd   = pos * radius * 0.7f + center;
        nvgBeginPath(vg);
        nvgMoveTo(vg, posStart.x, posStart.y);
        nvgLineTo(vg, posEnd.x, posEnd.y);
        nvgStrokeColor(vg, theme->getColor(indColor));
        nvgStrokeWidth(vg, math::max(1.0f, roundf((radius / 8.0f) * 2.0f) / 2.0f));
        nvgStroke(vg);
        nvgLineCap(vg, NVGlineCap::NVG_BUTT);
    }
}

void guiknob::setToDefaultValue() {
#if BUILD_DAW_HOST
    fModifyBeginValue = lastVal = getValue();
    if (fnValueEditBegin) {
        fnValueEditBegin(fModifyBeginValue, fModifyBeginValue);
    }
    float newVal = fDefaultValue;
    if (paramAutomatable) {
        paramAutomatable->resetParamValue(paramIdx, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
        newVal = getValue();
        setValue(newVal, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
        paramAutomatable->postSetParameter(paramIdx, fModifyBeginValue, getValue(), FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
    } else {
        setValue(fDefaultValue, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
    }
    if (fnValueEditFinish) {
        fnValueEditFinish(fModifyBeginValue, newVal);
    }
#else
    setValue(fDefaultValue, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
#endif
}

void guiknob::setKnobInternalHandlers() {
#if BUILD_DAW_HOST
    fnGetValue = [this]() {
        if (paramAutomatable) {
            return paramAutomatable->getParamValue(paramIdx);
        }
        return value;
    };
    fnSetValue = [this](float f, int flags) {
        if (paramAutomatable) {
            //TODO: lock external VST2 instances
            ThreadLock lock     = dawCtrl ? dawCtrl->lockPlayThread() : ThreadLock::MakeVoidLock();
            paramAutomatable->setParamEdit(paramIdx, f, flags);
        }
    };
    fnValueEditFinish = [this](float preVal, float val) {
        if (paramAutomatable) {
            paramAutomatable->postSetParameter(paramIdx, preVal, getValue(), FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
        }
    };
    if (dawCtrl)
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
    if (knobType != knobtype::KNOB_UNLABELED) {
        m_layout.labelHeight = math::roundfS32(math::max(8.0f, size.y * m_layout.scaleLabel));
        m_layout.valueHeight = math::roundfS32(math::max(8.0f, size.y * m_layout.scaleValue));
        const int INS_BRD    = m_layout.inset;
        m_layout.pLabel      = pos + ivec2(INS_BRD);
        m_layout.pValue      = pos + ivec2(INS_BRD, size.y - (INS_BRD + m_layout.valueHeight));
        m_layout.sLabel      = ivec2(size.x - 2 * INS_BRD, m_layout.labelHeight);
        m_layout.sValue      = ivec2(size.x - 2 * INS_BRD, m_layout.valueHeight);
        m_layout.pKnob       = pos + ivec2(INS_BRD, INS_BRD + m_layout.labelHeight);
        m_layout.sKnob       = size - ivec2(INS_BRD*2, m_layout.labelHeight + m_layout.valueHeight + 2 * INS_BRD);
    } else {
        m_layout.labelHeight = 0;
        m_layout.valueHeight = 0;
        m_layout.pLabel      = pos;
        m_layout.pValue      = pos;
        m_layout.sLabel      = {};
        m_layout.sValue      = {};
        m_layout.pKnob       = pos;
        m_layout.sKnob       = size;
    }
}

void guiknob_labeled_base::render(NVGcontext* vg) {

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
    if (m_layout.sKnob.x > 0 && m_layout.sKnob.y > 0) {
        renderButtonAt(vg, m_layout.pKnob, m_layout.sKnob, value);
    }
    if (m_layout.renderLabelBorder) {
        if (m_layout.sLabel.x > 0 && m_layout.sLabel.y > 0) {
            renderBorder(vg, getStateFlags(), m_layout.pLabel, m_layout.sLabel, GuiColor::COL_BG_BRT);
        }
        if (m_layout.sValue.x > 0 && m_layout.sValue.y > 0) {
            renderBorder(vg, getStateFlags(), m_layout.pValue, m_layout.sValue, GuiColor::COL_BG_BRT);
        }
    }
    NVGcolor fontColor;
    if (isBackgroundRendered()) {
        auto bgColor       = theme->getColor(getBackgroundColor());
        fontColor = getContrastFontColor(nvgToRGB(bgColor));
    } else {
        fontColor = theme->getColor(getLabelColor());
    }
    auto minPos = math::minvec2f(m_layout.pLabel, m_layout.pValue);
    auto sMax = math::maxvec2f(m_layout.pLabel + m_layout.sLabel, m_layout.pValue + m_layout.sValue) - minPos;
    if (sMax.x > 0 && sMax.y > 0) {
        nvgSave(vg);
        nvgIntersectScissor(vg, minPos.x, minPos.y, sMax.x, sMax.y);
        nvgFillColor(vg, fontColor);
        if (m_layout.sLabel.x > 0 && m_layout.sLabel.y > 0) {
            float x = renderTextLabel(vg, vec2(m_layout.pLabel) + vec2(m_layout.sLabel) * 0.5f, m_layout.sLabel, label, theme, m_layout.labelHeight * m_layout.fontScaleLabel, fontColor, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            float right = m_layout.sLabel.x;
            if (x > right) {
                if (m_layout.fontScaleLabel > 0.5f) {
                    m_layout.fontScaleLabel -= 0.05f;
                }
            }
        }
        if (m_layout.sValue.x > 0 && m_layout.sValue.y > 0) {
            float x = renderTextLabel(vg, vec2(m_layout.pValue) + vec2(m_layout.sValue) * 0.5f, m_layout.sValue, valueDisplay, theme, m_layout.valueHeight * m_layout.fontScaleValue, fontColor, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            float right = m_layout.sValue.x;
            if (x > right) {
                if (m_layout.fontScaleValue > 0.5f) {
                    m_layout.fontScaleValue -= 0.05f;
                }
            }
        }
        nvgRestore(vg);
    }
}

void guiknob::handleRightClick(MouseEvent& evt) {
#if BUILD_DAW_HOST
    if (dawCtrl && paramAutomatable && paramIdx > -1) {
        if (parentCtrl) {
            dbgassert(paramAutomatable->getParam(paramIdx));
            parentCtrl->openContextMenu(new guictxtmenu_at_param(dawCtrl, paramAutomatable, paramIdx), evt.mousepos);
        }
        return;
    }
#endif
    if (parent)
        parent->rightClicked(evt, this);
}
void gui_slider_textfield::handleRightClick(MouseEvent& evt) {
    dbgassert(paramAutomatable && paramIdx > -1 && paramAutomatable->getParam(paramIdx));
    parentCtrl->openContextMenu(new guictxtmenu_at_param(dawCtrl, paramAutomatable, paramIdx), evt.mousepos);
}
bool gui_slider_textfield::isAutomated() {
    dbgassert(paramAutomatable && paramIdx > -1 && paramAutomatable->getParam(paramIdx));
    auto at = paramAutomatable->getRegisteredAutomation(paramIdx);
    return at && at->isAutomated();
}
void gui_slider_textfield::setColors() {
    if (isHighlighted()) {
        valColor = GuiColor::COL_KNOB_HIGHLIGHT;
        indColor = GuiColor::COL_KNOB_HIGHLIGHT;
    } else if (isModulated()) {
        valColor = GuiColor::COL_KNOB_MODULATED;
        indColor = GuiColor::COL_KNOB_MODULATED;
    } else if (isAutomated()) {
        valColor = GuiColor::COL_AUTOMATED;
        indColor = GuiColor::COL_AUTOMATED;
    } else {
        indColor = GuiColor::COL_KNOB_IND;
        valColor = GuiColor::COL_KNOB;
    }
}
void gui_slider_textfield::render(NVGcontext* vg) {
    renderWidgetBorder(vg, getStateFlags());
    setColors();
    if (paramAutomatable && paramIdx > -1) {
        vec2 insetP        = vec2(pos + 1);
        vec2 insetS        = vec2(size - 2);
        float fParamScaled = getRenderScaledValue(paramAutomatable->getParamValue(paramIdx));
        float x            = insetP.x;
        float y            = insetP.y;
        float rectWidth;
        if (renderAsBipolar()) {
            // render bipolar: fParamScaled is 0..1
            // make sure rectWidth is not negative
            if (fParamScaled < 0.5f) {
                x         = insetP.x + insetS.x * fParamScaled;
                rectWidth = insetS.x * (0.5f - fParamScaled);
            } else {
                x         = insetP.x + insetS.x * 0.5f;
                rectWidth = insetS.x * (fParamScaled - 0.5f);
            }
        } else {
            rectWidth = (fParamScaled) *insetS.x;
        }
        if (rectWidth > 0.45f) {
            nvgBeginPath(vg);
            nvgRect(vg, x, y, rectWidth, insetS.y);
            nvgFillColor(vg, theme->getColor(valColor));
            nvgFillCustomPar(vg, -3);
            nvgFill(vg);
        }
        float textWidth = 0;
        if (isTextCommitted()) {
            const String strLvl = getValueAsString(paramAutomatable->getParamValue(paramIdx));
            textWidth           = renderTextLabel(vg,
                                                  insetP + insetS * 0.5f,
                                                  insetS,
                                                  strLvl,
                                                  theme,
                                                  fontSize(),
                                                  theme->getColor(getLabelColor()),
                                                  NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
        if (isFlag(FLG_RENDER_LABEL) && this->label.length()) {
            renderTextLabel(vg,
                            insetP + vec2(3.0f, insetS.y * 0.5f),
                            vec2(insetS.x - textWidth - 6.0f, insetS.y),
                            label,
                            theme,
                            fontSize() * FONT_AUTOSCALE,
                            theme->getColor(GuiColor::COL_LABEL_INACTIVE),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }
    if (!isTextCommitted()) {
        gui_textfield::render(vg);
    }
}
bool gui_slider_textfield::handleCharInput(uint32_t codepoint) {
    if (isTextCommitted() && codepoint < 0xFF) {
        char keyChar = (char) codepoint;
        if ((keyChar >= '0' && keyChar <= '9') || (keyChar == '-')) {
            MouseHitEvt evt(MouseHitType::MOUSE_LEFT, 0);
            gui_textfield::setValue(getValueAsString(paramAutomatable->getParamValue(paramIdx)));
            gui_textfield::focusEvent(evt, true);
            gui_textfield::setSelectionRange(-1, -1);
        }
    }
    if (!isTextCommitted()) {
        return gui_textfield::handleCharInput(codepoint);
    }
    return false;
}
bool gui_slider_textfield::keyboardEvent(int key, int scancode, KeyEventType action, int modifiers) {

    if (action == KeyEventType::K_PRESS && isTextCommitted()) {
        if ((key == KEY_ENTER || key == KEY_KP_ENTER)) {
            MouseHitEvt evt(MouseHitType::MOUSE_LEFT, 0);
            gui_textfield::setValue(getValueAsString(paramAutomatable->getParamValue(paramIdx)));
            gui_textfield::focusEvent(evt, true);
            gui_textfield::setSelectionRange(-1, -1);
        }
    }

    if (!isTextCommitted()) {
        return gui_textfield::keyboardEvent(key, scancode, action, modifiers);
    }
    if (action == KeyEventType::K_PRESS || action == KeyEventType::K_REPEAT) {
        if (key == KEY_UP) {
            float amt = -1.0f;
            if (modifiers == KB_MOD_SHIFT) {
                amt *= 0.1f;
            }
            updateAutomatableParam(amt, false);
            return true;
        } else if (key == KEY_DOWN) {
            float amt = 1.0f;
            if (modifiers == KB_MOD_SHIFT) {
                amt *= 0.1f;
            }
            updateAutomatableParam(amt, false);
            return true;
        }
    }
    return false;
}
void gui_slider_textfield::onTextEndEdit() {
    float fNew = parseTextValue(gui_textfield::value());
    auto flags = param_update_flags::FLG_PAR_UPDATE_FINISH | param_update_flags::FLG_PAR_UPDATE_USER;
    paramAutomatable->setParamEdit(paramIdx, fNew, flags);
}
void gui_slider_textfield::handleDraggedBegin(MouseEvent& evt) {
    if (!isTextCommitted()) {
        gui_textfield::handleDraggedBegin(evt);
        return;
    }
    if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
        MouseHitEvt mouseHitEvt(MouseHitType::MOUSE_LEFT, 0);
        gui_textfield::setValue(getValueAsString(paramAutomatable->getParamValue(paramIdx)));
        gui_textfield::focusEvent(mouseHitEvt, true);
        gui_textfield::setSelectionRange(-1, -1);
        return;
    }
    if (evt.guiDragged == this) {
        parentCtrl->captureMouse(this);
    }
}
void gui_slider_textfield::handleDraggedMove(MouseEvent& evt) {
    if (!isTextCommitted()) {
        gui_textfield::handleDraggedMove(evt);
        return;
    }
    if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
        int scale = isCtrl(evt.kbmods) ? 15 : 2;
        int disty = (int) evt.dragDistance->y / scale;
        if (!disty)
            return;

        evt.dragDistance->y = 0;
        if (paramAutomatable && paramIdx > -1) {
            updateAutomatableParam(disty * 0.1f, true);
        }
    }
}
void gui_slider_textfield::handleDraggedRelease(MouseEvent& evt) {
    if (!isTextCommitted()) {
        gui_textfield::handleDraggedRelease(evt);
        return;
    }
}
void gui_slider_textfield::updateAutomatableParam(float amt, bool applyUserInputScaling) {
    float fNew = modifyParam(paramAutomatable->getParamValue(paramIdx), amt, applyUserInputScaling);
    auto flags = param_update_flags::FLG_PAR_UPDATE_USER;
    paramAutomatable->setParamEdit(paramIdx, fNew, flags);
}
float gui_slider_textfield::parseTextValue(const String& str) {
    auto param             = paramAutomatable->getParam(paramIdx);
    param_unit_t paramUnit = { str, param->unit };
    auto parsed            = paramAutomatable->convertParamValueDisplay(paramIdx, paramUnit);
    return parsed.floatVal;
}
float gui_slider_textfield::modifyParam(float param, float amt, bool applyUserInputScaling) {
    if (applyUserInputScaling) {
        amt *= 0.01f;
    }
    return math::clamp(param - amt, 0.0f, 1.0f);
}

bool gui_slider_textfield::isModulated() {
    if (paramAutomatable) {
        return DAW::IsParamModulated(paramAutomatable, paramIdx);
    }
    return false;
}
