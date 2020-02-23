#pragma once
#include "math/vec.h"
#include "str_util.h"
#include "knob.h"
#include "guicontainer.h"
#include "button.h"
#include <vector>


class gui_ctr_debug : public guictr_base {
public:
	enum class gui_ctr_debug_type_i32 : int32_t {
		TYPE_0, TYPE_1, TYPE_2
	};
private:
	const gui_ctr_debug_type_i32 dgbCtrType;
	int32_t curVal = 0;
	std::vector<guibase*> debugGuis;
	std::vector<String> g_debugStrings;
public:
	gui_ctr_debug(gui_ctr_debug_type_i32 debugCtrType);
	~gui_ctr_debug() {
		removeGuis();
		for (auto* g : debugGuis) {
			delete g;
		}
	}
	virtual void render(NVGcontext* vg);
	void layout();
	void addStr(String str) {
		g_debugStrings.push_back(std::move(str));
	}
	void buttonClicked(guibase* button);
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
};
