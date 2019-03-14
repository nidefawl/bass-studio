#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include "glheaders.h"
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
using glm::vec2;
using glm::ivec2;

#include "str_util.h"
#include "color_util.h"
#include "gui/gui.h"
#include "gui/button.h"
#include "gui/knob.h"
#include "gui/inputfield.h"
#include "gui/guicontainer.h"
#include "gui/contextmenus.h"
#include "basectrl.h"
#include "platform.h"
#include "plugins/plugin.h"
#include "host/plugin/base_plugin.h"
#include "host/plugin/vst_plugin.h"
#ifdef BUILD_BUILTIN_EFFECT
#include "host/mainctrl.h"
#endif

#include "stereowidth-plugin.h"
#include "stereowidth-pluginctr.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"
#include "leak_detect.h"

using namespace PluginStereoWidth;

using namespace std;
class guiknob_labeled : public guiknob {
	int labelHeight = 0;
	int valueHeight = 0;
	String valueDisplay = "";
	const int button_inset = 10;
	AudioEffect* curEffect = nullptr;
	int32_t internalEffectIdx = 0;
#ifdef BUILD_BUILTIN_EFFECT
	vstplugin* hostSidePlugin = nullptr;
#endif
public:
	guiknob_labeled(int _paramIdx, int _internalEffectIdx) : guiknob(false) {
#ifdef BUILD_BUILTIN_EFFECT
		paramIdx = _paramIdx;
#endif
		internalEffectIdx = _internalEffectIdx;
		fnValueEditChanged = [this](float preVal, float val) {
			if (curEffect) {
				curEffect->setParameterAutomated(internalEffectIdx, val);
				setDisplayValueFromEffect();
			}
		};
#ifdef BUILD_BUILTIN_EFFECT
		fnFocus = [this](MouseHitEvt& evt, bool focused) {focusEvent(evt, focused);};
		setAutomationHandlers();
#endif
	}
	~guiknob_labeled() {
	}
#ifdef BUILD_BUILTIN_EFFECT
	void setEffectInstance(vstplugin* _hostSidePlugin) {
		hostSidePlugin = _hostSidePlugin;
		paramAutomatable = _hostSidePlugin;
	}
    virtual bool focusEvent(MouseHitEvt& evt, bool focused) override {
    	if (focused && paramAutomatable) {
    		MainCtrl* ctrl = dynamic_cast<MainCtrl*>(getControl());
			assert(ctrl);
    		if (ctrl) {
        		ctrl->showAutomation(hostSidePlugin->getTrack(), hostSidePlugin, paramIdx);
    		}
    	}
    	return true;
    }
	void handleRightClick(MouseEvent& evt) override {
    	if (this->hostSidePlugin) {
    		MainCtrl* ctrl = dynamic_cast<MainCtrl*>(getControl());
			assert(ctrl);
    		if (ctrl) {
    			automatable_param_t* paramRef = &hostSidePlugin->params[paramIdx];
    			assert(paramRef);
        		ctrl->openContextMenu(new guictxtmenu_vstparam(this->hostSidePlugin, paramRef), evt.mousepos);
    		}
    	}
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			if (evt.type != MouseHitType::MOUSE_RIGHT)
			{
				if (guiknob::mouseHitTest(mpos, evt)) {
					return true;
				}
			}
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
#endif
	void layout() override {
		int buttonSize = size.x - button_inset*2;
		int left = (size.y-buttonSize);
		float scaleTop = 0.35f;
		float scaleBottom = 0.25f;
		labelHeight = std::max(14.0f, left * scaleTop);
		valueHeight = std::max(14.0f, left * scaleBottom);
	}
	virtual void render(NVGcontext* vg) {
		updateAutomationState(vg);
//		nvgBeginPath(vg);
//		nvgRect(vg, pos.x, pos.y, size.x, size.y);
//		nvgFillColor(vg, GUI_COLORRGB(150, 150, 200, 180));
//		nvgFill(vg);
		ivec2 insetP = pos+ivec2(button_inset, labelHeight);
		ivec2 insetS = size-ivec2(button_inset*2, labelHeight+valueHeight);
		const int INS_BRD = 2;
//		renderWidgetBorder(vg);
//		renderWidgetBorderPosSize(vg, getStateFlags(), pos + ivec2(0, labelHeight+INS_BRD),
//				size - ivec2(0, labelHeight+valueHeight+INS_BRD*2));
		renderWidgetBorderPosSize(vg, getStateFlags(), pos + ivec2(0, +INS_BRD),
				ivec2(size.x, labelHeight-INS_BRD*2));
		renderWidgetBorderPosSize(vg, getStateFlags(), pos + ivec2(0, size.y - valueHeight+INS_BRD),
				ivec2(size.x, valueHeight-INS_BRD*2));
		auto bgColor = theme->getBgColor(getStateFlags());
		auto contrastColor = getContrastFontColor(nvgToRGB(bgColor));
		renderButtonAt(vg, insetP, insetS);
		setFont(vg, G_FONT_SCALE(labelHeight-2), G_WHITE, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, contrastColor);
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + G_FONT_MIDDLE_OFFSET(labelHeight), StringAsCStr(label), NULL);
		nvgFontSize(vg, G_FONT_SCALE(valueHeight-2));
		nvgText(vg, pos.x + size.x / 2.0f, pos.y + size.y - valueHeight + G_FONT_MIDDLE_OFFSET(valueHeight), StringAsCStr(valueDisplay), NULL);
	}
	void setAudioEffect(AudioEffect* eff) {
		this->curEffect = eff;
		if (eff) {
			setValueInit(eff->getParameter(internalEffectIdx));
			setLabel(eff->getParameterName(internalEffectIdx));
		}
		setDisplayValueFromEffect();
	}
	void setDisplayValueFromEffect() {
		if (this->curEffect) {
			String display = curEffect->getParameterDisplay(internalEffectIdx);
			String displayUnit = curEffect->getParameterLabel(internalEffectIdx);
			this->valueDisplay = display+displayUnit;
		} else {

			this->valueDisplay = "???";
		}
	}
};

class guicontainer_stereowidth : public guictr_base {
	vstplugin* vstHostSide = nullptr;
	AudioEffect* curEffect = nullptr;
	guiknob_labeled knobgain;
	guiknob_labeled knobwidth;

public:
	guicontainer_stereowidth();
	~guicontainer_stereowidth() {
		remove(&knobgain);
		remove(&knobwidth);
	}
	std::vector<String> g_debugStrings;
	virtual void render(NVGcontext* vg);
	virtual void prerender(NVGcontext* vg);
	virtual void onTick(AppCtrl* ctrl) override;
	void layout();
	void buttonClicked(guibase* button) override;
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
	void setVSTPlugin(vstplugin* vstHostSide);

	guiknob_labeled* getKnobFromParameter(int32_t index) {
		switch (index) {
			case kGain:
				return &knobgain;
			case kStereoWidth:
				return &knobwidth;
		}
		return nullptr;
	}
	void onSetParameter(int32_t index, float value) {
		guiknob_labeled* knob = getKnobFromParameter(index);
		if (knob && curEffect) {
			knob->setValueInit(value);
			knob->setDisplayValueFromEffect();
		}
	}
};


guicontainer_stereowidth::guicontainer_stereowidth()
: guictr_base(), knobgain(1+kGain, kGain), knobwidth(1+kStereoWidth, kStereoWidth) {
	padding = 4;
	margin = 4;
	add(&knobwidth);
	add(&knobgain);
}
void guicontainer_stereowidth::onGuiOpen(AudioEffect* eff) {
	this->curEffect = eff;
	knobwidth.setAudioEffect(eff);
	knobgain.setAudioEffect(eff);
}
void guicontainer_stereowidth::onGuiClose(AudioEffect* eff) {
	this->curEffect = nullptr;
}
void guicontainer_stereowidth::setVSTPlugin(vstplugin* vstHostSide)  {
	this->vstHostSide = vstHostSide;
#ifdef BUILD_BUILTIN_EFFECT
	knobwidth.setEffectInstance(vstHostSide);
	knobgain.setEffectInstance(vstHostSide);
#endif
}
void guicontainer_stereowidth::onTick(AppCtrl* ctrl) {
	for (guibase* gui : guis) {
		gui->onTick(ctrl);
	}
}
void guicontainer_stereowidth::buttonClicked(guibase* button) {
}
void guicontainer_stereowidth::prerender(NVGcontext* vg) {
	for (guibase* gui : guis) {
		gui->prerender(vg);
	}
}

void guicontainer_stereowidth::render(NVGcontext* vg) {
//	nvgBeginPath(vg);
//	nvgRect(vg, pos.x, pos.y, size.x, size.y);
//	nvgFillColor(vg, GUI_COLORRGB(50, 50, 150, 180));
//	nvgFill(vg);
	renderBackground(vg);
	if (!setScissorTransform(vg)) {
		return;
	}
//	ivec2 cs = getSizeContent();
//	nvgBeginPath(vg);
//	nvgRect(vg, 0, 0, cs.x, cs.y);
//	nvgFillColor(vg, GUI_COLORRGB(50, 150, 150, 180));
//	nvgFill(vg);
//	nvgBeginPath(vg);
//	const int INS = 2;
//	nvgRect(vg, INS, INS, cs.x-INS*2, cs.y-INS*2);
//	nvgFillColor(vg, GUI_COLORRGB(150, 150, 50, 180));
//	nvgFill(vg);

	for (guibase* gui : guis) {
		nvgSave(vg);
		gui->render(vg);
		nvgRestore(vg);
	}

}
void guicontainer_stereowidth::layout() {
	ivec2 cs = getSizeContent();
	const int inset = 4;
	const int knobSize = max(32, (cs.x-inset*3)/2);
	knobwidth.size = ivec2(knobSize, cs.y-inset*2);
	knobgain.size = ivec2(knobSize, cs.y-inset*2);
	knobwidth.pos = ivec2(inset);
	knobgain.pos = ivec2(knobwidth.right()+inset, inset);
	for (guibase* gui : guis) {
		gui->layout();
	}
}
bool guicontainer_stereowidth::handleKeyInput(KeyEvent& event) {
	if (event.type != KeyEventType::K_RELEASE) {

	}
	return false;
}


class ViewContainersStereoWidth : public PluginViewContainersImpl {
public:
	guicontainer_stereowidth ctr_main;
	ViewContainersStereoWidth() : PluginViewContainersImpl(220, 150)
	{
	}
	virtual ~ViewContainersStereoWidth() {
	}
	void layout(int32_t winW, int32_t winH) override {
		ctr_main.pos = {0, 0};
		ctr_main.size = {winW, winH};
	}
	void addTo(std::vector<guictr_base*>& v) override {
		 v.push_back(&ctr_main);
	}
	void onGuiOpen(AudioEffect* eff) override {
		ctr_main.onGuiOpen(eff);
	}
	void onGuiClose(AudioEffect* eff) override {
		ctr_main.onGuiClose(eff);
	}
	void onSetParameter(int32_t index, float value) override {
		ctr_main.onSetParameter(index, value);
	}
	void getFixedSize(int32_t* w, int32_t* h) override {
		*w = this->width;
		*h = this->height;
	}
	void setVSTPlugin(vstplugin* hostsideplugin)  {
		ctr_main.setVSTPlugin(hostsideplugin);
	}
};
namespace PluginStereoWidth {
	AudioEffectX* createPlugin (audioMasterCallback audioMaster)
	{
		return new PluginVST2_StereoWidth (audioMaster);
	}
	PluginViewContainers* PluginVST2_StereoWidth::createView() {
		PluginViewContainers* pviewctr = new ViewContainersStereoWidth();
		this->views.push_back(pviewctr);
		return pviewctr;
	}
}



