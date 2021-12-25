#pragma once
#include "guicontainer.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "event.h"
#include "button.h"
#include "knob.h"
#include "host/midiarp.h"

class gui_arp : public guictr_base {
public:
    String text;
    guibuttontoggle buttonBypass;
    clip_view& clipview;
    guiknob clock;
    guiknob gate;
    guiknob pattern;
    guiknob randTime;
    guiknob randTmMode;
    guiknob randVel;
    guiknob* knobs[6] = { &pattern, &clock, &gate, &randTime, &randTmMode, &randVel };
    midiarp* getArp() {

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
        add(&buttonBypass);
        for (guiknob* knob: knobs) {
            add(knob);
        }
        padding = 2;
        margin  = 0;
        text    = "Arpeggiator";
        clock.setLabel("Clock");
        gate.setLabel("Gate");
        pattern.setLabel("Pattern");
        randTime.setLabel("Random Time");
        randTmMode.setLabel("Random Time Mode");
        randVel.setLabel("Random Velocity");
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
        for (guiknob* knob: knobs) {
            knob->setAutomationHandlers();
        }
    }
    void buttonClicked(guibase* _button);
    void rightClicked(MouseEvent& evt, guibase* button) override;
    virtual ~gui_arp() {
        removeGuis();
    }
    virtual void render(NVGcontext* vg) {
        if (!setScissorTransformContainer(vg)) {
            return;
        }
        renderFrameBase(vg);
        int flags = parentCtrl->isCtrOrChildFocused(this) ? FLAG_FOCUSED : 0;
        renderTitleBar(vg, size, this->text, GuiConstant::CONST_FIXED_TITLE_HEIGHT, buttonBypass.right(), flags, true);
        renderFrameOutline(vg);
        buttonBypass.render(vg);
        midiarp* arp = getArp();
        if (arp) {
            for (guiknob* knob: knobs) {
                knob->render(vg);
            }
            for (guiknob* knob: knobs) {
                nvgBeginPath(vg);
                int32_t widthParam = this->getSizeContent().x - knob->right() - INSET_TITLE * 2;
                nvgRect(vg, knob->right() + INSET_TITLE, knob->pos.y, widthParam, knob->size.y);
                nvgFillColor(vg, theme->getFrameColorHighlight());
                nvgFill(vg);
                String text = knob->label;
                if (text[0]) {
                    setFont(vg, (int) ((knob->size.y / 2.0)), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
                    nvgText(vg, knob->right() + INSET_TITLE + INSET_TITLE, knob->pos.y + INSET_TITLE, StringAsCStr(text), NULL);
                    text = formatParameterValue(knob);
                    if (text[0]) {
                        setFont(vg, (int) ((knob->size.y / 2.0) * 0.8), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
                        nvgText(vg, knob->right() + INSET_TITLE + INSET_TITLE, knob->bottom() - INSET_TITLE, StringAsCStr(text), NULL);
                    }
                }
            }
        }
    }

    String formatParameterValue(guiknob* knob) {
        midiarp* arp = getArp();
        if (!arp) return "";
        if (knob == &clock) {
            return StringFormat("%d ticks", arp->getStepSize());
        }
        if (knob == &pattern) {
            int32_t option = arp->getPatternIdx();
            if (option == 0) {
                return "Chord";
            }
            return StringFormat("%d", option);
        }
        if (knob == &gate) {
            return StringFormat("%d ticks", arp->getDuration());
            //			return StringFormat("%.2f %%", math::clamp(arp->getGateF()*100.0f, 0.0f, 100.0f));
        }
        if (knob == &randVel) {
            return StringFormat("+/-%d", arp->getRandVelocity());
        }
        String upDown = arp->getRandTmMode() ? "+/-" : "+";
        if (knob == &randTime) {
            return StringFormat("%s%d ticks", StringAsCStr(upDown), arp->getRandTime());
        }
        if (knob == &randTmMode) {
            return StringAsCStr(upDown);
        }
        return "";
    }

    virtual void layout() override {
        const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
        int buttonSize    = hpt * 0.8;
        int32_t inset1    = (hpt - buttonSize) / 2;
        buttonBypass.size = { buttonSize, buttonSize };
        buttonBypass.pos  = { inset1, inset1 };
        buttonBypass.setRadius(hpt / 3.f);

        guiknob* knobPrev = nullptr;
        for (guiknob* knob: knobs) {
            knob->size = ivec2(48);
            knob->pos  = ivec2(INSET_TITLE, (knobPrev ? knobPrev->bottom() : hpt) + INSET_TITLE);
            knobPrev   = knob;
        }
        for (guibase* gui: guis) {
            gui->layout();
        }
    }
    void showEditClip() {
#if BUILD_VSTHOST
        auto arp = getArp();
        clock.setAutomationRef(arp, ARP_PARAM_CLOCK);
        gate.setAutomationRef(arp, ARP_PARAM_GATE);
        pattern.setAutomationRef(arp, ARP_PARAM_PATTERN);
        randTime.setAutomationRef(arp, ARP_PARAM_RAND_TIME);
        randTmMode.setAutomationRef(arp, ARP_PARAM_RAND_MODE);
        randVel.setAutomationRef(arp, ARP_PARAM_RAND_VEL);
#endif
    }
};
