#pragma once
#include <vector>
#include "event.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "scrollbar.h"
#include "basectrl.h"

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;


#define INSET_CTXT_MENU_X 1
#define INSET_CTXT_MENU_Y 2
static const ivec2 insetCtxtMenu = ivec2(INSET_CTXT_MENU_X, INSET_CTXT_MENU_Y);

class guictr_scrollbar : public guictr_base, public gui_scrollcontainer {
	gui_scrollbar scrollbar;
public:
	int contentHeight = 0;
	bool hasScrollbar = false;
	bool scrollbarOutside = false;
	int maxHeight = 360;
	guictr_scrollbar() : guictr_base(), scrollbar(1, 0.0f, *this) {
		padding = 0;
	}
	guictr_scrollbar(guibase* gui) : guictr_scrollbar() {
		add(gui);
	}
	~guictr_scrollbar() {
	}
	virtual void render(NVGcontext* vg);
	void determineSize() override;
	void layout() override;
	void onChildLayoutChanged(guibase* g) override;
	bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override;

	ivec2 getScrollTotalSize() override {
		ivec2 cs = getSizeContent();
		cs.y = contentHeight;
		return cs;
	}
	ivec2 getScrollViewSize() override {
		return getSizeContent();
	}
	void scrollOffsetChanged(int dir, float offset) {
		if (hasScrollbar) {
			for (guibase* gui : guis) {
				if (gui == &scrollbar)
					continue;
				gui->pos.y = -offset*(contentHeight-size.y);
			}
		}
	}
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
		return scrollbar.handleMouseScroll(evt, xoffset, yoffset);
	}
	virtual void setControl(BaseCtrl* parentCtrl) override {
		guictr_base::setControl(parentCtrl);
		scrollbar.setControl(parentCtrl);
	}
};

