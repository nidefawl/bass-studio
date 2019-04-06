#pragma once
#include "gui/gui.h"

struct gui_shaderview_impl_t;
class gui_shaderview : public guibase {
	gui_shaderview_impl_t* impl = nullptr;
public:
	gui_shaderview();
	~gui_shaderview();
	void prerender(NVGcontext* vg) override;
	void render(NVGcontext* vg) override;
	void onTick(AppCtrl* appctrl) override;
};

