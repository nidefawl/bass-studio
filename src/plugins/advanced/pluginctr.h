#pragma once
#include <vector>
#include <memory>

#include "str_util.h"
#include "gui/knob.h"
#include "gui/button.h"
#include "gui/guicontainer.h"
#include "gui/inputfield.h"


class gui_color_pick : public guictr_base {
	guiknob knH;
	guiknob knS;
	guiknob knL;
	NVGcolor rgbColor;
public:
	gui_color_pick()
	: guictr_base(),
	  knH(false),
	  knS(false),
	  knL(false) {
		padding=0;
		margin=0;
		add(&knH);
		add(&knS);
		add(&knL);
		init();
	}
	void init();
	~gui_color_pick() {
		remove(&knH);
		remove(&knS);
		remove(&knL);
	}
	void layout() {
		for (auto* g : guis)
			g->size = vec2(48);
		knH.pos = vec2(48, 0);
		knS.pos = vec2(knH.right(), 0);
		knL.pos = vec2(knS.right(), 0);
		for (auto* g : guis)
			g->layout();
	}
	void render(NVGcontext* vg) override {
		if (!setScissorTransform(vg)) {
			return;
		}
		for (auto* g : guis)
			g->render(vg);
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, 48, 48);
		nvgFillColor(vg, this->rgbColor);
		nvgFill(vg);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	void handleRightClick(MouseEvent& evt);
};
class vstplugin;
class AudioEffect;
class gui_ctr_main : public guictr_base {
	vstplugin* vstHostSide = nullptr;
	AudioEffect* curEffect = nullptr;
	gui_color_pick knobTest;
	guibutton btnLoop;
//	gui_timeinput clipTimeStart;
	gui_numberinput_field field;
	gui_textfield textField;
	int nr;

public:
	gui_ctr_main();
	~gui_ctr_main() {
		remove(&field);
		remove(&textField);
		remove(&knobTest);
	}
	std::vector<String> g_debugStrings;
	virtual void render(NVGcontext* vg);
	virtual void prerender(NVGcontext* vg);
	virtual void onTick(AppCtrl* ctrl) override;
	void buttonClicked(guibase* button) override;
	void layout();
	void addStr(String str) {
		g_debugStrings.push_back(std::move(str));
	}
	bool handleKeyInput(KeyEvent& kevt) override;
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			if (evt.type == MouseHitType::MOUSE_LEFT) {
				evt.requestFocus(this);
				return true;
			}
		}
		return false;
	}
	void onGuiOpen(AudioEffect* eff);
	void onGuiClose(AudioEffect* eff);
	void onSetParameter(int32_t index, float value);
	void setVSTPlugin(vstplugin* vstHostSide);
};
