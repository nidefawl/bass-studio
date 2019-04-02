#include "glheaders.h"
#include <nanovg.h>
#include <nanovg_gl.h>
#include <nanovg_gl_utils.h>
#include <vector>
#include <memory>

#include "str_util.h"
#include "color_util.h"
#include "platform.h"
#include "gui/gui.h"
#include "gui/guicontainer.h"
#include "gui/knob.h"
#include "gui/button.h"
#include "gui/guicontainer.h"
#include "gui/guicolorpick.h"
#include "gui/guiinputfield.h"
#include "gui/pluginviewcontainers.h"

#include "basectrl.h"

#include "plugins/plugin.h"
#include "host/plugin/vst_plugin.h"

#include "empty-plugin.h"
#include "vstsdk-plugin-2.4/audioeffect.h"
#include "vstsdk-plugin-2.4/audioeffectx.h"





class vstplugin;
class AudioEffect;

namespace {

class guictr_emptyvst : public guictr_base {
	vstplugin* vstHostSide = nullptr;
	AudioEffect* curEffect = nullptr;
public:
	guictr_emptyvst() : guictr_base() {
		setBackgroundRendered(true);
	}
	~guictr_emptyvst() {
		removeGuis();
	}
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

	void render(NVGcontext* vg) override {
		if (isBackgroundRendered()){
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
		setFont(vg, 26, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		nvgText(vg, 5, 25, "EMPTY VST", NULL);
	#ifdef NO_GLFW_LIB
		nvgText(vg, 5, 75, "NO GLFW", NULL);
	#else
		nvgText(vg, 5, 75, "USING GLFW", NULL);
	#endif

	}
	bool handleKeyInput(KeyEvent& event) override {
		if (event.type != KeyEventType::K_RELEASE) {

		}
		return false;
	}

	void onGuiOpen(AudioEffect* eff) {
		this->curEffect = eff;
	}
	void onGuiClose(AudioEffect* eff) {
		this->curEffect = nullptr;
	}
	void setVSTPlugin(vstplugin* vstHostSide)  {
		this->vstHostSide = vstHostSide;
	}

	void onSetParameter(int32_t index, float value) {
	}
};


class ViewContainersEmptyPlugin : public PluginViewContainersImpl {
public:
	guictr_emptyvst ctr_main;
	ViewContainersEmptyPlugin() : PluginViewContainersImpl(400, 300)
	{
	}
	virtual ~ViewContainersEmptyPlugin() {
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

}

namespace PluginEmptyVST2 {
	AudioEffectX* createPlugin (audioMasterCallback audioMaster) {
		return new EmptyPluginVST2 (audioMaster);
	}
	PluginViewContainers* EmptyPluginVST2::createView() {
		return new ViewContainersEmptyPlugin();
	}
}
