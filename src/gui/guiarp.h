#pragma once
#include "math/vec.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "event.h"
#include "str_util.h"
#include "color_util.h"
#include "button.h"
#include "renderresources.h"
#include "list.h"
#include "guimeter.h"
#include "knob.h"
#include "track_impl.h"
#include "../host/midiarp.h"
#include "../host/mainctrl.h"

class gui_arp : public guictr_base {
public:
	String text;
	guibuttontoggle buttonBypass;
	clip_view& clipview;
	guiknob clock;
	guiknob gate;
	guiknob pattern;
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
	gui_arp(clip_view& _clipview) : clipview(_clipview) {
		add(&buttonBypass);
		add(&clock);
		add(&gate);
		add(&pattern);
		padding = 2;
		margin = 0;
		text = "Arpeggiator";
		clock.setLabel("Clock");
		gate.setLabel("Gate");
		pattern.setLabel("Pattern");
		buttonBypass.setRadius(12);
		buttonBypass.icon = ICON_BYPASS;
		buttonBypass.setParent(this);
		buttonBypass.colorActive = GuiColor::COL_BTN_BG_BYPASS_ACTIVE;
		buttonBypass.getState = [this]() {
			auto arp = getArp();
			if (arp) {
				return arp->getParamValue(0)>0;
			}
			return false;
		};
		guiknob* knobs[3] { &clock, &gate, &pattern };
		for (guiknob* knob : knobs) {
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
		guiknob* knobs[3] = {&clock, &gate, &pattern};
		midiarp* arp = getArp();
		if (arp) {
			for (guiknob* knob : knobs) {
				knob->render(vg);
			}
			for (guiknob* knob : knobs) {
				nvgBeginPath(vg);
				int32_t widthParam = this->getSizeContent().x - knob->right() - INSET_TITLE*2;
				nvgRect(vg, knob->right()+INSET_TITLE, knob->pos.y, widthParam, knob->size.y);
				nvgFillColor(vg, theme->getFrameColorHighlight());
				nvgFill(vg);
				String text = knob->label;
				if (text[0]) {
					setFont(vg, (int)((knob->size.y/2.0)), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
					nvgText(vg, knob->right()+INSET_TITLE+INSET_TITLE, knob->pos.y+INSET_TITLE, StringAsCStr(text), NULL);
					text = formatParameterValue(knob);
					if (text[0]) {
						setFont(vg, (int)((knob->size.y/2.0)*0.8), G_WHITE, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
						nvgText(vg, knob->right()+INSET_TITLE+INSET_TITLE, knob->bottom()-INSET_TITLE, StringAsCStr(text), NULL);
					}
				}
			}
		}
	}

	String formatParameterValue(guiknob* knob) {
		midiarp* arp = getArp();
		if (!arp) return "";
		if (knob == &clock) {
			return StringFormat("%.2f", arp->getClockF());
		}
		if (knob == &pattern) {
			return StringFormat("%.2f", arp->getPatternF());
		}
		if (knob == &gate) {
			return StringFormat("%.2f", arp->getGateF());
		}
		return "";
	}

	virtual void layout() override {
		const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
		int buttonSize = hpt * 0.8;
		int32_t inset1 = (hpt - buttonSize) / 2;
		buttonBypass.size = {buttonSize, buttonSize};
		buttonBypass.pos = {inset1, inset1};
		buttonBypass.setRadius(hpt/3.f);

		clock.size = ivec2(48);
		gate.size = ivec2(48);
		pattern.size = ivec2(48);
		clock.pos = ivec2(INSET_TITLE, hpt+INSET_TITLE);
		gate.pos = ivec2(INSET_TITLE, clock.bottom()+INSET_TITLE);
		pattern.pos = ivec2(INSET_TITLE, gate.bottom()+INSET_TITLE);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	void showEditClip() {
#if BUILD_VSTHOST
		auto arp = getArp();
		clock.setAutomationRef(arp, ARP_PARAM_CLOCK);
		gate.setAutomationRef(arp, ARP_PARAM_GATE);
		pattern.setAutomationRef(arp, ARP_PARAM_PATTERN);
#endif
	}
};
