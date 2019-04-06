#pragma once
#include <list>
#include <vector>
#include <set>
#include <stdint.h>
#include <memory>

#include "math/vec.h"
#include "config.h"
#include "str_util.h"
#include "basectrl.h"
#include "window.h"
#include "menu.h"
#include "mouse.h"
#include "keyboard.h"
#include "event.h"
#include "logging.h"
#include "plugin.h"
#include "../gui/pluginviewcontainers.h"

struct NVGcontext;
class guibase;
class guictr_base;
class guictxtmenu_base;
class AudioEffect;


KeyEvent keyEvent(int key, int scancode, int keyState, int mods, const char* key_name);

class PluginControl : public AppCtrl
{
	PluginViewContainersImpl* view;
	bool firstInit = true;
public:
	PluginControl(PluginViewContainersImpl* view);
	~PluginControl();
	static PluginControl* get();
	void focusReceived() override { };
	void focusLost() override { };

    void mouseMoved(ivec2 mousePos, ivec2 deltaPos) override;
	void menuCommand(int cmd) override;
	void onTick() override;
	bool init(window_main* window, NVGcontext* nanovg) override;
	void postInit() override;
	void destroy() override;
	void relayout(int32_t w, int32_t h) override;
	bool processGlobalKeyevent(KeyEvent& event) override;
	bool mouseDownPre() override;

	void initApp(int argc, char* argv[]) override;

	void onGuiOpen(AudioEffect* eff);
	void onGuiClose(AudioEffect* eff);
	void onSetParameter(int32_t index, float value);
};
