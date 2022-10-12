#include "arp.h"
#include "gui/automation/automatable.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/controls/knoblabeled.h"
#include "seq_util.h"

class guiknob_arp : public guiknob {
    const DAW::midiarp::arp_param_entry_t& param;
    public:
    explicit guiknob_arp(const DAW::midiarp::arp_param_entry_t& _param)
        : guiknob(guiknob::knobtype::KNOB_UNLABELED), param(_param) {}
    void setArp(DAW::midiarp* arp) {
        setAutomationRef(arp, param.id);
    }
};

gui_arp::gui_arp(clip_view& _clipview)
    : clipview(_clipview) {
    setCanMouseHit(true);
    add(&buttonBypass);
    auto outIt = std::begin(knobs);
    for (auto& param : DAW::midiarp::parameterTypes) {
        if (param.id >= PARAM_OFFSET_IMPL) {
            auto knob = new guiknob_arp(param);
            knob->setLabel(param.name);
            *outIt++ = knob;
            add(knob);
        }
    }
    editfield.setFlag(FLG_NO_LAYOUT, true);
    editfield.setVisible(false);
    editfield.setAlignment(gui_textfield::Alignment::Center);
    editfield.setReturnCommits(true);
    add(&editfield);
    buttonBypass.setRadius(12);
    buttonBypass.icon = ICON_BYPASS;
    buttonBypass.setParent(this);
    buttonBypass.colorActive = GuiColor::COL_BTN_BG_BYPASS_ACTIVE;
    buttonBypass.fnGetState  = [this]() {
        auto arp = getArp();
        if (arp) {
            return arp->getParamValue(PARAM_ENABLE) > 0;
        }
        return false;
    };
    for (guiknob* knob : knobs) {
        knob->setKnobInternalHandlers();
    }
}

gui_arp::~gui_arp() {
    removeGuis();
    for (guiknob* knob : knobs) {
        delete knob;
    }
}

void gui_arp::buttonClicked(guibase* _button) {
    if (_button == &buttonBypass) {
        auto* arp = getArp();
        if (arp) {
            ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
            toggleDeviceEnableState(arp, FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
        }
    }
    if (stl_contains(knobs, _button)) {
        auto button = dynamic_cast<guiknob_arp*>(_button);
        auto* arp = getArp();
        if (button && arp) {
            auto paramIdx = button->getParamIdx();
            auto paramValue = arp->getParamValueDisplay(paramIdx);
            editfield.mCallbackEnd = [this, button, arpBegin = arp, paramValue, paramIdx](const std::string& str) {
                auto* arp = getArp();
                if (arp && arpBegin == arp) {
                    auto paramConverted = arp->convertParamValueDisplay(paramIdx, param_unit_t{str, paramValue.unit});
                    if (paramConverted.success) {
                        arp->setParamValue(paramIdx, paramConverted.floatVal, FLG_PAR_UPDATE_USER);
                        if (button->fnValueEditChanged)
                            button->fnValueEditChanged(button->getValue(), paramConverted.floatVal);
                    }
                    editfield.setVisible(false);
                }
                return true;
            };
            editfield.pos = button->getRightTop() + ivec2{0, button->size.y / 2};
            editfield.size = {getSizeContent().x - editfield.pos.x / 2, button->size.y / 2};
            editfield.setVisible(true);
            editfield.layout();
            editfield.setValue(paramValue.value);
            editfield.setSelectionRange(-1, -1);
            editfield.setFontSize(editfield.size.y * theme->getFloat(GuiConstant::CONST_FONT_SCALE));
            parentCtrl->focusGui(&editfield);
            return;
        }
    }
}

void gui_arp::rightClicked(MouseEvent& evt, guibase* button) {
    int32_t clickedParamIdx = -1;
    if (button == &this->buttonBypass) {
        clickedParamIdx = PARAM_ENABLE;
    }
    if (clickedParamIdx != -1) {
        auto* ctxt = new guictxtmenu_at_param(this->dawCtrl, this->getArp(), clickedParamIdx);
        parentCtrl->openContextMenu(ctxt, evt.mousepos);
    }
}

void gui_arp::handleDraggedBegin(MouseEvent& evt) {
    if ((isCtrl(evt.kbmods) || (evt.type == MouseEventType::M_EVT_DOUBLECLICK))) {
        for (guiknob* knob : knobs) {
            if (knob->contains(ivec2{ knob->left() + knob->size.x / 2, evt.relMousepos.y })) {
                buttonClicked(knob);
                return;
            }
        }
    }
    guictr_base::handleDraggedBegin(evt);
}

DAW::midiarp* gui_arp::getArp() {
    track_t* track = clipview.track();
    if (track) {
        auto audio = track->audio;
        if (audio) {
            return audio->arp;
        }
    }
    return nullptr;
}

bool gui_arp::setScissorTransformContainer(NVGcontext* vg) {
    ivec2 sizeInset = getSizeContent();
    if (sizeInset.y <= 0 || sizeInset.x <= 0) {
        return false;
    }
    nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
    nvgTranslate(vg, pos.x, pos.y);
    return true;
}

void gui_arp::render(NVGcontext* vg) {
    if (!setScissorTransformContainer(vg)) {
        return;
    }
    renderFrameBase(vg);
    auto* arp = getArp();
    String title;
    if (arp) {
        title = arp->getAutomatableName();
    }
    int flags = parentCtrl->isCtrOrChildFocused(this) ? TITLEBAR_FLG_FOCUSED : TITLEBAR_FLG_NONE;
    if (isSelected()) flags |= TITLEBAR_FLG_SELECTED;
    renderTitleBar(vg, size, title, GuiConstant::CONST_PLUGIN_TITLE_HEIGHT, buttonBypass.right(), flags, true);
    renderFrameOutline(vg);
    if (buttonBypass.isVisible())
        buttonBypass.render(vg);
    ivec2 posInset = getPosContent();
    nvgTranslate(vg, posInset.x - pos.x, posInset.y - pos.y);
    nvgTranslateZ(vg, -4.0f);
    if (arp) {
        ivec2 cs = getSizeContent();
        for (guiknob* knob : knobs) {
            knob->render(vg);
        }
        for (guiknob* knob : knobs) {
            nvgBeginPath(vg);
            int32_t widthParam = cs.x - knob->right() - INSET_TITLE * 2;
            nvgRect(vg, knob->right() + INSET_TITLE, knob->pos.y, widthParam, knob->size.y);
            nvgFillColor(vg, theme->getFrameColorHighlight());
            nvgFill(vg);
            if (!knob->label.empty()) {
                auto textPos    = vec2(knob->right() + INSET_TITLE + INSET_TITLE, knob->pos.y + knob->size.y * 0.25f);
                auto textBounds = vec2(widthParam, knob->size.y * 0.5f);
                renderText(vg, textPos, textBounds, knob->label, textBounds.y);

                auto text       = arp->getParamValueDisplay(knob->getParamIdx());
                String textUnit = text.value;
                if (!text.unit.empty()) {
                    textUnit += " " + text.unit;
                }
                if (!textUnit.empty()) {
                    textPos.y += knob->size.y * 0.5f;
                    renderText(vg, textPos, textBounds, textUnit, textBounds.y * 0.8f);
                }
            }
        }
        if (editfield.isVisible()) {
            editfield.render(vg);
        }
    }
}

void gui_arp::layout() {
    padding           = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
    const auto hpt    = static_cast<float>(theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT));
    auto buttonSize   = hpt * 0.8f;
    auto inset1       = (hpt - buttonSize) * 0.5f;
    buttonBypass.size = ivec2(math::roundfS32(buttonSize));
    buttonBypass.pos  = ivec2(math::roundfS32(inset1));
    buttonBypass.setRadius(hpt / 3.f);
    guiknob* knobPrev = nullptr;
    for (guiknob* knob : knobs) {
        knob->size = ivec2(48);
        knob->pos  = ivec2(padding, (knobPrev ? knobPrev->bottom() : hpt) + padding);
        knobPrev   = knob;
    }
    for (guibase* gui : guis) {
        gui->layout();
    }
}

void gui_arp::showEditClip() {
    auto arp = getArp();
    for (auto* knob : knobs) {
        knob->setArp(arp);
    }
}
