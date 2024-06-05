#include "knob.h"
#include "assert_dbg.h"
#include "basectrl.h"
#include "event.h"
#include "gui/controls/textfield.h"
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
#include "host/automation/automation.h"
#include "host/daw/mainctrl.h"
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
    auto ptr = getInstanceOrNull();
    if (!ptr) {
        return;
    }
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
void guiknob::storeEditModulationTransform(NVGcontext* vg) {
    if (dawCtrl && dawCtrl->getIsContainerRenderPass() && DAW::UI::Modulation::IsEditModulation(this, paramAutomatable, paramIdx)) {
        DawCtrl::ui_modulation_targets_t t;
        nvgSaveState(vg, &t.state);
        t.target = toRef();
        dawCtrl->getUIModulationTargets().push_back(t);
    }
}
void guiknob::render(NVGcontext* vg) {
    storeEditModulationTransform(vg);
    if (!isRenderableSizeAndContext(vg))
        return;
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
            lastVal             = this->value;
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
static void renderSlider2(NVGcontext* vg, guitheme_t* theme, const vec2& posKn, const vec2& sizeKn, int sliderAxisDir, float fRenderValue, bool bIsBipolar, const NVGcolor& color) {
    vec2 rectPos = {};
    vec2 rectSize = {};
    bool bSaturated = math::abs(fRenderValue*2.0f - 1.0f) > 1.0f;
    fRenderValue = math::clamp(fRenderValue, 0.0f, 1.0f);
    if (!bIsBipolar) {
        rectSize = sizeKn * fRenderValue;
        rectPos  = posKn;
        if (sliderAxisDir == 1) {
            rectPos.y += sizeKn.y - rectSize.y;
            rectSize.x = sizeKn.x;
        } else {
            rectSize.y = sizeKn.y;
        }
    } else {
        float biVal = fRenderValue * 2.0f - 1.0f;
        float fRenderValueAbs = math::abs(biVal);
        if (biVal < 0) {
            rectPos = posKn + sizeKn * 0.5f;
            rectSize = sizeKn * 0.5f * fRenderValueAbs;
        } else {
            rectPos = posKn + sizeKn * 0.5f * (1.0f - fRenderValueAbs);
            rectSize = sizeKn * 0.5f * fRenderValueAbs;
        }
        rectPos[1-sliderAxisDir] = posKn[1-sliderAxisDir];
        rectSize[1-sliderAxisDir] = sizeKn[1-sliderAxisDir];
    }
    if (rectSize[sliderAxisDir] > 0.45f) {
        nvgBeginPath(vg);
        nvgRect(vg, rectPos.x, rectPos.y, rectSize.x, rectSize.y);
        nvgFillColor(vg, color);
        nvgFillCustomPar(vg, -3);
        nvgFill(vg);
        if (bSaturated) {
            nvgBeginPath(vg);
            nvgRect(vg, rectPos.x, rectPos.y, rectSize.x, rectSize.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_MODULATION_SATURATED));
            nvgFillCustomPar(vg, -4);
            nvgFill(vg);
        }
    }
}
void renderRoundKnob(NVGcontext* vg, float cx, float cy, float radius, float start, float range, bool bIsBipolar, float fScaled, const NVGcolor& color, float lineThickness) {
    float rangeScaled = bIsBipolar ? range * 0.5f : range;
    float startOffset = bIsBipolar ? start + rangeScaled : start;

    float end = startOffset + fScaled * rangeScaled;
    if (fabs(fScaled) > 1E-8F) {
        float endArc = end;
        if (endArc < startOffset) {
            std::swap(startOffset, endArc);
        }
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, radius, startOffset, endArc, NVG_CW);
        nvgStrokeColor(vg, color);
        nvgStrokeWidth(vg, lineThickness + 1.0f);
        nvgFillCustomPar(vg, -3);
        nvgStrokeCustomPar(vg, -3);
        nvgStroke(vg);
    }
}
static float getParamByType(const automatable_param_t* param, int type) {
    float fParam = 0.0f;
    switch (type) {
        case 0:
            fParam = param->getValue();
            break;
        case 1:
            fParam = param->getValueAutomated();
            break;
        case 2:
            fParam = param->getValueModulated();
            break;
    }
    return fParam;
}
float guiknob::getParamScaled(const automatable_param_t* param, int type) {
    float fParam = getParamByType(param, type);
    fParam = math::clamp(fParam, 0.0f, 1.0f);
    if (param->isBiPolar && knobType != knobtype::SLIDER_LABELED) {
        fParam = fParam * 2.0f - 1.0f;
    }
    return fParam;
}
void guiknob::renderButtonAt(NVGcontext* vg, ivec2 insetP, ivec2 insetS, float value) {
    // setColors();
    renderWidgetBorder(vg, getStateFlags());

    NVGcolor c2 = theme->getColor(GuiColor::COL_BG_BRT);
    if (hovered())
        c2 = theme->getColor(GuiColor::COL_BG_DRKER);
    if (focused())
        c2 = theme->getColor(GuiColor::COL_BG_DRKER2);

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
    int sliderAxisDir = 1;
    float fScaled = 0.0f;

    float lineThickness = math::max(1.0f, roundf((minSize / 8.0f) * 2.0f) / 2.0f);
    float radius        = (minSize * 0.8f) / 2.0f;
    cx = insetP.x + insetS.x / 2.0f;
    cy = insetP.y + insetS.y / 1.8f;
    vec2 center(cx, cy);
    if (knobType == knobtype::KNOB_LABELED || knobType == knobtype::KNOB_UNLABELED) {
        renderRoundKnob(vg, cx, cy, radius, start, range, false, 1.0f, theme->getColor(GuiColor::COL_TEXT), lineThickness);
    }
    if (paramAutomatable) {
        auto param = paramAutomatable->getParam(paramIdx);
        if (!assert_expr(param)) {
            return;
        }
        bIsBipolar |= param->isBiPolar;
        int32_t type = 0;
        auto autLane = paramAutomatable->getRegisteredAutomation(param->idx);
        if (knobType == knobtype::SLIDER_LABELED) {
            renderSlider2(vg, theme, insetP, insetS, sliderAxisDir, getParamScaled(param, type), param->isBiPolar, theme->getColor(GuiColor::COL_KNOB));
        } else {
            renderRoundKnob(vg, cx, cy, radius, start, range, param->isBiPolar, getParamScaled(param, type), theme->getColor(GuiColor::COL_KNOB), lineThickness + 1.5f);
        }
        if (autLane && autLane->isActive()) {
            type = 1;
            if (knobType == knobtype::SLIDER_LABELED) {
                renderSlider2(vg, theme, insetP, insetS, sliderAxisDir, getParamScaled(param, type), param->isBiPolar, theme->getColor(GuiColor::COL_AUTOMATED));
            } else {
                renderRoundKnob(vg, cx, cy, radius+1.0f, start, range, param->isBiPolar, getParamScaled(param, type), theme->getColor(GuiColor::COL_AUTOMATED), lineThickness - 2.0f);
            }
        }
        if (param->isModulated()) {
            if (!paramAutomatable->isBypassModulation()) {
                type = 2;
            }
            float fScaledModulated = getParamScaled(param, type);
            const auto bIsModulationHighlighted = DAW::UI::Modulation::IsEditModulation(this, paramAutomatable, paramIdx);
            if (bIsModulationHighlighted) {
                float highLightMinVal = 0.025f;
                if (param->isBiPolar) {
                    if (math::abs(fScaledModulated - 0.5f) < highLightMinVal) {
                        fScaledModulated = 0.5f+highLightMinVal;
                    }
                } else {
                    if (fScaledModulated < highLightMinVal) {
                        fScaledModulated = highLightMinVal;
                    }
                }
            }
            if (knobType == knobtype::SLIDER_LABELED) {
                renderSlider2(vg, theme, insetP, insetS, sliderAxisDir, fScaledModulated, param->isBiPolar, theme->getColor(GuiColor::COL_KNOB_MODULATED));
            } else {
                renderRoundKnob(vg, cx, cy, radius-1.0f, start, range, param->isBiPolar, fScaledModulated, theme->getColor(GuiColor::COL_KNOB_MODULATED), lineThickness - 2.0f);
            }
        }
        float fTextValue = 0.0f;
        if (parentCtrl->getGuiOverRef() == toRef()) {
            fTextValue = getParamByType(param, 0);
            fScaled = getParamScaled(param, 0);
        } else {
            fTextValue = getParamByType(param, type);
            fScaled = getParamScaled(param, type);
        }
        auto paramUnit = paramAutomatable->convertParamValueToDisplay(paramIdx, fTextValue);
        strValueDisplay = paramUnit.unit.empty() ? paramUnit.value : paramUnit.value + " " + paramUnit.unit;
    } else{
        fScaled = math::clamp(value, bIsBipolar ? -1.0f : 0.0f, 1.0f);
        if (knobType == knobtype::SLIDER_LABELED) {
            renderSlider2(vg, theme, insetP, insetS, sliderAxisDir, fScaled, bIsBipolar, theme->getColor(GuiColor::COL_KNOB));
        } else {
            renderRoundKnob(vg, cx, cy, radius, start, range, bIsBipolar, fScaled, theme->getColor(GuiColor::COL_KNOB), lineThickness);
        }
    }
    if (knobType == knobtype::SLIDER_LABELED) {
        auto modRangesOptional = getKnobModulationRanges();
        if (modRangesOptional) {
            const auto& modRanges = modRangesOptional.value();
            const auto numMods = CtrSize(modRanges);
            dbgassert(numMods);
            for (int i = 0; i < numMods; i++) {
                auto& param = modRanges[i];
                float fScaledBi = fScaled;
                if (bIsBipolar) {
                    fScaledBi = 0.5f + (fScaled - 0.5f);
                }
                auto posModulation = height - height * float(fScaledBi);
                NVGcolor color = dbgcolorsArray[1 + (param.sourceId % (dbgcolorsArraySize-1))];
                color.a = 0.5f;
                auto heightModulation = height * float(param.range) * (param.isBiPolar ? 2.0f : 1.0f);
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
                    nvgRect(vg, insetP.x + xSlot, insetP.y + r.y, wSlot, r.w);
                    dbgassert(r.x >= 0 && r.y >= 0 && r.z <= insetS.x && r.w <= insetS.y);
                    nvgFillColor(vg, color);
                    nvgFill(vg);
                }
            }
        }
        float lineThickness = math::max(1.0f, roundf((minSize / 32.0f) * 2.0f) / 2.0f);
        float heightHandle = math::max(3.0f, lineThickness + 3.0f);
        auto posKn = vec2(insetP);
        auto sizeKn = vec2(insetS);
        if (sliderAxisDir == 1) {
            posKn = posKn + vec2(0, sizeKn.y * (1.0f - fScaled) - heightHandle * 0.5f);
            sizeKn = vec2(sizeKn.x, heightHandle);
        } else {
            posKn = posKn + vec2(sizeKn.x * fScaled - heightHandle * 0.5f, 0);
            sizeKn = vec2(heightHandle, sizeKn.y);
        }
        nvgBeginPath(vg);
        nvgRect(vg, posKn.x, posKn.y, sizeKn.x, sizeKn.y);
        c2.a = 0.5f;
        nvgFillColor(vg, c2);
        nvgFill(vg);
    } else {
        float rangeScaled = bIsBipolar ? range * 0.5f : range;
        float startOffset = bIsBipolar ? start + rangeScaled : start;
        float end = startOffset + fScaled * rangeScaled;
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
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_KNOB_IND));
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
            return paramAutomatable->getParam(paramIdx)->getValue();
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
            auto guiTrackCtr = dawCtrl->getTrackContainer();
            if (!guiTrackCtr)
                return;
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
    storeEditModulationTransform(vg);
    if (!isRenderableSizeAndContext(vg))
        return;

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
    strValueDisplay = "N/A";
    if (m_layout.sKnob.x > 0 && m_layout.sKnob.y > 0) {
        renderButtonAt(vg, m_layout.pKnob, m_layout.sKnob, value);
    }
    if (fnOverrideGetDisplay) {
        strValueDisplay = fnOverrideGetDisplay(value);
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
            float right = m_layout.pLabel.x + m_layout.sLabel.x;
            if (x > right) {
                if (m_layout.fontScaleLabel > 0.5f) {
                    m_layout.fontScaleLabel -= 0.05f;
                }
            }
        }
        if (m_layout.sValue.x > 0 && m_layout.sValue.y > 0) {
            float x = renderTextLabel(vg, vec2(m_layout.pValue) + vec2(m_layout.sValue) * 0.5f, m_layout.sValue, strValueDisplay, theme, m_layout.valueHeight * m_layout.fontScaleValue, fontColor, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            float right = m_layout.pValue.x + m_layout.sValue.x;
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
    guibase::handleRightClick(evt);
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
static void renderSlider(NVGcontext* vg, const vec2& insetP, const vec2& insetS, float fRenderValue, bool bIsBipolar, const NVGcolor& color) {
    float rectWidth = 0;
    float x         = insetP.x;
    float y         = insetP.y;
    if (bIsBipolar) {
        if (fRenderValue < 0.5f) {
            x         = insetP.x + insetS.x * fRenderValue;
            rectWidth = insetS.x * (0.5f - fRenderValue);
        } else {
            x         = insetP.x + insetS.x * 0.5f;
            rectWidth = insetS.x * (fRenderValue - 0.5f);
        }
    } else {
        rectWidth = (fRenderValue) *insetS.x;
    }
    if (rectWidth > 0.45f) {
        nvgBeginPath(vg);
        nvgRect(vg, x, y, rectWidth, insetS.y);
        nvgFillColor(vg, color);
        nvgFillCustomPar(vg, -3);
        nvgFill(vg);
    }
}

void gui_slider_textfield::render(NVGcontext* vg) {
    if (dawCtrl && dawCtrl->getIsContainerRenderPass() && DAW::UI::Modulation::IsEditModulation(this, paramAutomatable, paramIdx)) {
        DawCtrl::ui_modulation_targets_t t;
        nvgSaveState(vg, &t.state);
        t.target = toRef();
        dawCtrl->getUIModulationTargets().push_back(t);
    }
    if (!isRenderableSizeAndContext(vg))
        return;
    vec2 insetP        = vec2(pos + 1);
    vec2 insetS        = vec2(size - 2);
    if (insetS.x < 2 || insetS.y < 2)
        return;
    renderWidgetBorder(vg, getStateFlags());
    if (paramAutomatable && paramIdx > -1) {
        auto param = paramAutomatable->getParam(paramIdx);
        if (!assert_expr(param)) {
            return;
        }
        auto autLane = paramAutomatable->getRegisteredAutomation(param->idx);
        float fBaseValue = param->getValue();
        float fRenderValue = fBaseValue;
        float fParam = math::clamp(fRenderValue, 0.0f, 1.0f);
        renderSlider(vg, insetP, insetS, getRenderScaledValue(fParam), param->isBiPolar, theme->getColor(GuiColor::COL_KNOB));
        if (autLane && autLane->isActive()) {
            fRenderValue = param->getValueAutomated();
            fParam = math::clamp(fRenderValue, 0.0f, 1.0f);
            renderSlider(vg, insetP, insetS, getRenderScaledValue(fParam), param->isBiPolar, theme->getColor(GuiColor::COL_AUTOMATED));
        }
        if (param->isModulated()) {
            fRenderValue = param->getValueModulated();
            fParam = math::clamp(fRenderValue, 0.0f, 1.0f);
            float fScaled = getRenderScaledValue(fParam);
            const auto bIsModulationHighlighted = DAW::UI::Modulation::IsHiglightedModulation(this, paramAutomatable, paramIdx);
            if (bIsModulationHighlighted) {
                float highLightMinVal = 0.025f;
                if (param->isBiPolar) {
                    if (math::abs(fScaled - 0.5f) < highLightMinVal) {
                        fScaled = 0.5f+highLightMinVal;
                    }
                } else {
                    if (fScaled < highLightMinVal) {
                        fScaled = highLightMinVal;
                    }
                }
            }
            renderSlider(vg, insetP, insetS, fScaled, param->isBiPolar, theme->getColor(GuiColor::COL_KNOB_MODULATED));
        }
        float textWidth = 0;
        if (isTextCommitted()) {
            float fTextValue = fRenderValue;
            if (parentCtrl->getGuiOverRef() == toRef()) {
                fTextValue = fBaseValue;
            }
            const String strLvl = getValueAsString(fTextValue);
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
            MouseHitEvt evt(MouseHitType::MOUSE_LEFT, KeyboardMods::KB_MODS_NONE);
            gui_textfield::setValue(getValueAsString(paramAutomatable->getParam(paramIdx)->getValue()));
            gui_textfield::focusEvent(evt, true);
            gui_textfield::setSelectionRange(-1, -1);
        }
    }
    if (!isTextCommitted()) {
        return gui_textfield::handleCharInput(codepoint);
    }
    return false;
}

bool gui_slider_textfield::keyboardEvent(KeyboardKey key, int scancode, KeyboardState action, KeyboardMods modifiers) {

    if (action == KeyboardState::K_PRESS && isTextCommitted()) {
        if ((key == KeyboardKey::DAW_KB_ENTER || key == KeyboardKey::DAW_KB_KP_ENTER)) {
            MouseHitEvt evt(MouseHitType::MOUSE_LEFT, modifiers);
            gui_textfield::setValue(getValueAsString(paramAutomatable->getParam(paramIdx)->getValue()));
            gui_textfield::focusEvent(evt, true);
            gui_textfield::setSelectionRange(-1, -1);
        }
    }

    if (!isTextCommitted()) {
        return gui_textfield::keyboardEvent(key, scancode, action, modifiers);
    }
    if (action == KeyboardState::K_PRESS || action == KeyboardState::K_REPEAT) {
        if (key == KeyboardKey::DAW_KB_UP) {
            float amt = -1.0f;
            if (modifiers == KB_MOD_SHIFT) {
                amt *= 0.1f;
            }
            updateAutomatableParam(amt, false, true);
            return true;
        } else if (key == KeyboardKey::DAW_KB_DOWN) {
            float amt = 1.0f;
            if (modifiers == KB_MOD_SHIFT) {
                amt *= 0.1f;
            }
            updateAutomatableParam(amt, false, true);
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
    fBeginValue = paramAutomatable->getParam(paramIdx)->getValue();
    if (!isTextCommitted()) {
        gui_textfield::handleDraggedBegin(evt);
        return;
    }
    if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
        MouseHitEvt mouseHitEvt(MouseHitType::MOUSE_LEFT, evt.kbmods);
        gui_textfield::setValue(getValueAsString(paramAutomatable->getParam(paramIdx)->getValue()));
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
            updateAutomatableParam(disty * 0.1f, true, false);
        }
    }
}

void gui_slider_textfield::handleDraggedRelease(MouseEvent& evt) {
    if (!isTextCommitted()) {
        gui_textfield::handleDraggedRelease(evt);
        return;
    }
    float fNew = paramAutomatable->getParam(paramIdx)->getValue();
    auto flags = param_update_flags::FLG_PAR_UPDATE_USER | param_update_flags::FLG_PAR_UPDATE_FINISH;
    paramAutomatable->postSetParameter(paramIdx, fBeginValue, fNew, flags);
}

void gui_slider_textfield::updateAutomatableParam(float amt, bool applyUserInputScaling, bool isFinal) {
    float fNew = modifyParam(paramAutomatable->getParam(paramIdx)->getValue(), amt, applyUserInputScaling);
    int32_t flags = param_update_flags::FLG_PAR_UPDATE_USER;
    if (isFinal) {
        flags |= param_update_flags::FLG_PAR_UPDATE_FINISH;
    }
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
#if BUILD_DAW_HOST
        return DAW::IsParamModulated(paramAutomatable, paramIdx);
#endif
    }
    return false;
}

GuiColor::constant_t gui_slider_textfield::getBackgroundColor() const {
#if BUILD_DAW_HOST
    if (DAW::UI::Modulation::IsHiglightedModulation(this, paramAutomatable, paramIdx)) {
        return GuiColor::COL_KNOB_HIGHLIGHT_BACKGROUND;
    }
#endif
    return gui_textfield::getBackgroundColor();
}

GuiColor::constant_t guiknob::getBackgroundColor() const {
#if BUILD_DAW_HOST
    if (DAW::UI::Modulation::IsHiglightedModulation(this, paramAutomatable, paramIdx)) {
        return GuiColor::COL_KNOB_HIGHLIGHT_BACKGROUND;
    }
#endif
    return guibase::getBackgroundColor();
}

String gui_slider_textfield::getValueAsString(float param) {
    auto paramValDisplay = paramAutomatable->getParamValueDisplay(paramIdx);
    return paramValDisplay.value + paramValDisplay.unit;
}

bool gui_slider_textfield::renderAsBipolar() {
    if (paramAutomatable) {
        auto param = paramAutomatable->getParam(paramIdx);
        return param->isBiPolar;
    }
    return false;
};
std::optional<std::vector<param_modulation_range_t>> guiknob::getKnobModulationRanges() {
    return std::nullopt;
}

void gui_slider_textfield::setAutomationRef(automatable_t* _paramAutomatable, int32_t _paramIdx) {
    this->paramAutomatable = _paramAutomatable;
    this->paramIdx         = _paramIdx;
    if (paramAutomatable && this->tooltipText.empty()) {
        auto param = paramAutomatable->getParam(paramIdx);
        if (param) {
            setTooltipText(param->extensiveName.empty() ? param->name : param->extensiveName);
            setFlag(FLG_RENDER_LABEL, true);
            label = param->name;
        }
    } else {
        label = "";
        setTooltipText("");
        setFlag(FLG_RENDER_LABEL, false);
    }
}
