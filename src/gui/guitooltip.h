#pragma once

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>
#include <memory>
#include <numeric>


#include "gui.h"
#include "guicontextmenu.h"
#include "guicontainer.h"
#include "table.h"

#define FONT_SIZE_TOOLTIP_TITLE 18
#define FONT_SIZE_TOOLTIP_BIG 15
#define FONT_SIZE_TOOLTIP 16
template <typename T>
class guitooltip : public guictxtmenu_base {
protected:
	T* ptr;
	bool hadMouseMovement = false;
	tbl table;
public:
	guitooltip(T* _ptr) : guictxtmenu_base(), ptr(_ptr) {
		padding = 0;
		margin = 0;
		scrollbarOutside=true;
		maxHeight = 220;
	}
	~guitooltip() {
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		hadMouseMovement = true;
		if (contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	bool isTransient() override {
		return true;
	}
	bool canClose() override {
		return hadMouseMovement && !ctrl->isMouseInside();
	}
	virtual void clicked(int _id) {
		ctrl->close();
		parentCtrl->closeContextMenu();
	}
	virtual void handleDraggedBegin(MouseEvent& evt) {
		return;
	}
	void layout();
	void render(NVGcontext* vg) {
		if (!setScissorTransformContainer(vg)) {
			return;
		}
		setFont(vg, FONT_SIZE_TOOLTIP_TITLE, G_WHITE, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
		draw(table, vg, ivec2(INSET_TABLE), getSizeContent()-ivec2(INSET_TABLE<<1), FONT_SIZE_TOOLTIP);
	}
};
