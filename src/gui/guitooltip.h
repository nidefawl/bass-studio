#pragma once

#include "math/vec.h"
#include <memory>
#include <numeric>


#include "gui.h"
#include "guicontextmenu_base.h"
#include "guicontextmenu.h"
#include "guicontainer.h"
#include "gui/textfield.h"
#include "table.h"

#define FONT_SIZE_TOOLTIP_TITLE 18
#define FONT_SIZE_TOOLTIP_BIG 15
#define FONT_SIZE_TOOLTIP 16

template <typename T>
class guitooltip : public guictxtmenu {
protected:
	T* ptr;
	bool hadMouseMovement = false;
	Table::tbl table;
	gui_textfield textField;
public:
	guitooltip(T* _ptr) : guictxtmenu(), ptr(_ptr) {
		add(&textField);
		textField.setVisible(false);
		padding = 0;
		margin = 0;
		scrollbarOutside=true;
		maxHeight = 220;
	}
	~guitooltip() {
		removeGuis();
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
		return hadMouseMovement && !parentCtrl->isMouseInside();
	}
	virtual void clicked(int _id) {
		parentCtrl->closePopup();
	}
//	virtual void handleDraggedBegin(MouseEvent& evt) override {
//		return;
//	}
//	virtual void handleDraggedMove(MouseEvent& evt) {
//		return;
//	}
//	virtual void handleDraggedRelease(MouseEvent& evt) override {
//		return;
//	}
	void onTick(AppCtrl* appctrl) {
		layout();
	}
	void layout();
	void render(NVGcontext* vg) {
		if (!setScissorTransformContainer(vg)) {
			return;
		}
		setFont(vg, FONT_SIZE_TOOLTIP_TITLE, G_WHITE, NVG_ALIGN_LEFT|NVG_ALIGN_BOTTOM);
		Table::DrawTableNVG(table, vg, theme, ivec2(INSET_TABLE), getSizeContent()-ivec2(INSET_TABLE<<1), FONT_SIZE_TOOLTIP);
		if (textField.isVisible()) {
			textField.render(vg);
		}
	}
};
