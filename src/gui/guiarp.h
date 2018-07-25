#pragma once
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
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
#include "leak_detect.h"
#include "track_impl.h"
#include "../host/midiarp.h"

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
	gui_arp(clip_view& _clipview) : buttonBypass(12), clipview(_clipview) {
		padding = 2; margin = 0;
		text = "Arpeggiator";
		add(&buttonBypass);
		add(&clock);
		add(&gate);
		add(&pattern);
		buttonBypass.icon = ICON_BYPASS;
		buttonBypass.parent = this;
		buttonBypass.setColor(0x80c040);
		clock.setLabel("Clock");
		gate.setLabel("Gate");
		pattern.setLabel("Pattern");
		buttonBypass.getState = [this]() {
			auto arp = getArp();
			if (arp) {
				return arp->getParamValue(0)>0;
			}
			return false;
		};
		clock.fnGetValue = [this](void) {
			auto arp = getArp();
			if (arp) {
				return arp->getClockF();
			}
			return 0.0f;
		};
		pattern.fnGetValue = [this](void) {
			auto arp = getArp();
			if (arp) {
				return arp->getPatternF();
			}
			return 0.0f;
		};
		gate.fnGetValue = [this](void) {
			auto arp = getArp();
			if (arp) {
				return arp->getGateF();
			}
			return 0.0f;
		};
		clock.fnSetValue = [this](float f) {
			auto arp = getArp();
			if (arp) {
		    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
				arp->setClockF(f);
			}
		};
		pattern.fnSetValue = [this](float f) {
			auto arp = getArp();
			if (arp) {
		    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
				arp->setPatternF(f);
			}
		};
		gate.fnSetValue = [this](float f) {
			auto arp = getArp();
			if (arp) {
		    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
				arp->setGateF(f);
			}
		};
	}
	virtual ~gui_arp() {
		remove(&pattern);
		remove(&gate);
		remove(&clock);
		remove(&buttonBypass);
		my_printf("DSTR!\n",0);
	}
	virtual void render(NVGcontext* vg) {
		if (!setScissorTransformContainer(vg)) {
			return;
		}
		renderFrameBase(vg);
		renderTitleBarHorizontal(vg, this->text, buttonBypass.right());
		renderFrameOutline(vg);
		clock.render(vg);
		pattern.render(vg);
		gate.render(vg);
		buttonBypass.render(vg);
		guiknob* knobs[3] = {&clock, &gate, &pattern};
		for (guiknob* knob : knobs) {
			nvgBeginPath(vg);
			int32_t widthParam = this->getSizeContent().x - knob->right() - INSET_TITLE*2;
			nvgRect(vg, knob->right()+INSET_TITLE, knob->pos.y, widthParam, knob->size.y);
			nvgFillColor(vg, GUI_COLOR(G_S3));
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
	void buttonClicked(guibase* _button) {
		if (_button == &buttonBypass) {
			midiarp* arp = getArp();
			if (arp) {
		    	ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
				arp->flipParamValue(0);
			}
		}
	}

//	virtual void renderDragged(NVGcontext* vg, ivec2 mousepos) {
//		mousepos -= pos;
//		nvgTranslate(vg, mousepos.x, mousepos.y);
//		render(vg);
//	}
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
		ivec2 size = getSizeContent();
		int32_t meterW = 32;
		while (size.x < meterW * 16 && meterW > 16) {
			meterW -= 4;
		}
		int32_t inset1 = (HEIGHT_PLUGIN_TITLE - buttonBypass.size.y) / 2;
		ivec2 contentS(size.x - meterW, size.y-HEIGHT_PLUGIN_TITLE);
		ivec2 contentP(0, HEIGHT_PLUGIN_TITLE);
		buttonBypass.pos.y = inset1;
		buttonBypass.pos.x = inset1;
		clock.size = ivec2(48);
		gate.size = ivec2(48);
		pattern.size = ivec2(48);
		clock.pos = ivec2(INSET_TITLE, HEIGHT_PLUGIN_TITLE+INSET_TITLE);
		gate.pos = ivec2(INSET_TITLE, clock.bottom()+INSET_TITLE);
		pattern.pos = ivec2(INSET_TITLE, gate.bottom()+INSET_TITLE);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	void showEditClip() {
	}
};
