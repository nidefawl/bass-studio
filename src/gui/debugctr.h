#pragma once
#include "str_util.h"
#include "knob.h"
#include "guicontainer.h"
#include "button.h"
#include <vector>

class gui_ctr_debug : public guictr_base {

	guiknob knobTest;
	guibutton btn;
public:
	gui_ctr_debug();
	~gui_ctr_debug() {
		remove(&btn);
		remove(&knobTest);
	}
	std::vector<String> g_debugStrings;
	virtual void render(NVGcontext* vg);
	void layout();
	void addStr(String str) {
		g_debugStrings.push_back(std::move(str));
	}
	void buttonClicked(guibase* button);
};
