#pragma once
#include <vector>
#include <memory>

#include "str_util.h"

#include "../../gui/guiinputfield.h"
#include "gui/knob.h"
#include "gui/button.h"
#include "gui/guicontainer.h"
#include "gui/guicolorpick.h"


class vstplugin;
class AudioEffect;
class gui_ctr_main : public guictr_base {
	vstplugin* vstHostSide = nullptr;
	AudioEffect* curEffect = nullptr;
	gui_color_pick colorPicker;
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
		remove(&colorPicker);
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
