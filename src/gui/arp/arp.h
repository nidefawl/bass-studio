#pragma once
#include "gui/container/container.h"
#include "gui/controls/knoblabeled.h"
#include "gui/controls/textfield.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "event.h"
#include "gui/controls/button.h"
#include "gui/controls/knob.h"
#include "host/midiarp.h"
#include "math/seq_math.h"

class gui_arp : public guictr_base {
    class guiknob_arp : public guiknob {
        const DAW::midiarp::arp_param_entry_t& param;
        public:
        guiknob_arp(const DAW::midiarp::arp_param_entry_t& _param)
            : guiknob(guiknob::knobtype::KNOB_UNLABELED), param(_param) {}
        void setArp(DAW::midiarp* arp) {
            setAutomationRef(arp, param.id);
        }
    };
    gui_textfield editfield;
    String text;
    guibuttontoggle buttonBypass;
    clip_view& clipview;
    std::array<guiknob_arp*, 6> knobs;
        
public:
    DAW::midiarp* getArp() {
        track_t* track = clipview.track();
        if (track) {
            auto audio = track->audio;
            if (audio) {
                return audio->arp;
            }
        }
        return nullptr;
    }
    gui_arp(clip_view& _clipview)
        : clipview(_clipview) {
        text    = "Synth";
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

    void handleDraggedBegin(MouseEvent& evt) override {
        if ((isCtrl(evt.kbmods) || (evt.type == MouseEventType::M_EVT_DOUBLECLICK))) {
            for (guiknob* knob : knobs) {
                if (knob->contains(ivec2{knob->left()+knob->size.x/2, evt.relMousepos.y})) {
                    buttonClicked(knob);
                    return;
                }
            }
        }
        guictr_base::handleDraggedBegin(evt);
    }
    void buttonClicked(guibase* _button) override;
    void rightClicked(MouseEvent& evt, guibase* button) override;
    ~gui_arp() override {
        removeGuis();
    }
    bool setScissorTransformContainer(NVGcontext* vg) override {
        ivec2 sizeInset = getSizeContent();
        if (sizeInset.y <= 0 || sizeInset.x <= 0) {
            return false;
        }
        nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
        nvgTranslate(vg, pos.x, pos.y);
        return true;
    }
    void render(NVGcontext* vg) override {
        if (!setScissorTransformContainer(vg)) {
            return;
        }
        renderFrameBase(vg);
        int flags = parentCtrl->isCtrOrChildFocused(this) ? TITLEBAR_FLG_FOCUSED : TITLEBAR_FLG_NONE;
        if (isSelected()) flags |= TITLEBAR_FLG_SELECTED;
        renderTitleBar(vg, size, this->text, GuiConstant::CONST_PLUGIN_TITLE_HEIGHT, buttonBypass.right(), flags, true);
        renderFrameOutline(vg);
        if (buttonBypass.isVisible())
            buttonBypass.render(vg);
        ivec2 posInset  = getPosContent();
        nvgTranslate(vg, posInset.x-pos.x, posInset.y-pos.y);
        nvgTranslateZ(vg, -4.0f);
        auto* arp = getArp();
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
                    auto textPos = vec2(knob->right() + INSET_TITLE + INSET_TITLE, knob->pos.y + knob->size.y * 0.25f);
                    auto textBounds = vec2(widthParam, knob->size.y * 0.5f);
                    renderText(vg, textPos, textBounds, knob->label, textBounds.y);

                    auto text = arp->getParamValueDisplay(knob->getParamIdx());
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

    void layout() override {
        padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
        const auto hpt = static_cast<float>(theme->get(GuiConstant::CONST_PLUGIN_TITLE_HEIGHT));
        auto buttonSize = hpt * 0.8f;
        auto inset1 = (hpt - buttonSize) * 0.5f;
        buttonBypass.size = ivec2(math::roundfS32(buttonSize));
        buttonBypass.pos = ivec2(math::roundfS32(inset1));
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
    void showEditClip() {
#if BUILD_VSTHOST
        auto arp = getArp();
        for (auto* knob : knobs) {
            knob->setArp(arp);
        }
#endif
    }
};
