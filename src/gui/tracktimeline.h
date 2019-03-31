#pragma once
#include <nanovg.h>
#include "grid.h"
#include "event.h"
#include "guicontainer.h"

class guitrack_timeline : public guictr_base, grid_changed_cb {
	scaled_grid& grid;
public:
	ivec2 startDrag{ 0, 0 };
	int dragDirection = -1;
	double dragPosObjSpace = 0;
	guitrack_timeline(scaled_grid& _grid)
		: guictr_base(),
		grid(_grid)
	{
		grid.addCallback(this);
		padding = 0;
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

	void handleDraggedBegin(MouseEvent& evt);
	void handleDraggedMove(MouseEvent& evt);
	void handleDraggedRelease(MouseEvent& evt);
	void render(NVGcontext* vg);
	void layout() {
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	void gridChanged(scaled_grid& _grid) override {

	}
	virtual guibase* getFocusedControl() {
		return this->parent;
	}
	virtual guibase* getFocusedContainer() {
		return this->parent;
	}
};
