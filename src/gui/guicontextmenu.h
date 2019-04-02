#pragma once
#include <vector>
#include "math/vec.h"
#include "event.h"
#include "str_util.h"
#include "gui.h"
#include "guicolors.h"
#include "guicontextmenu_base.h"
#include "basectrl.h"

class ctxtmenu_entry;
class guictxtmenu : public guictxtmenu_base {
protected:
	std::vector<ctxtmenu_entry*> entries;
public:
	guictxtmenu() : guictxtmenu_base() {
		setBackgroundRendered(true);
		setBackgroundRenderedInset(false);
	}
	virtual ~guictxtmenu() {
		for (ctxtmenu_entry* e : entries) {
			delete e;
		}
	}
	void addEntry(ctxtmenu_entry* entry) {
		size.x = math::max(size.x, entry->width);
		entries.push_back(entry);
		entry->theme = theme;
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	virtual void clicked(int _id) {
		closeContextMenu();
	}
	virtual void handleDraggedBegin(MouseEvent& evt) {
		ivec2 local = evt.relMousepos;
		for (ctxtmenu_entry* e : entries) {
			int n = e->getClicked(size, local);
			if (n >= 0) {
				clicked(n);
				return;
			}
		}
		return;
	}
	void layout() {
		//TODO: figure out string width here to make life easier laying out context menus
		int y = paddingV;
		for (ctxtmenu_entry* e : entries) {
			e->layout(size, fontSize);
			e->y = y;
			y += e->height + paddingV;
		}
	}
	void determineSize(ivec2& prefSize) override {
		ivec2 newMaxSize = {size.x, paddingV};
		for (ctxtmenu_entry* e : entries) {
			newMaxSize.y += e->height + paddingV;
		}
		if (entries.empty()) {
			newMaxSize.y += paddingV;
		}
		prefSize = newMaxSize;
	}

	void render(NVGcontext* vg) {
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
		setScissorTransform(vg);
		int idx = 0;
		ivec2 mouse = parentCtrl->m_mousePos;
		mouse = toContainerSpace(mouse);
		for (ctxtmenu_entry* e : entries) {
			e->render(size, vg, idx, mouse);
			idx++;
		}
	}
	void setControl(BaseCtrl* parentCtrl) {
		guictxtmenu_base::setControl(parentCtrl);
		for (auto* g : entries) {
			g->theme = parentCtrl ? parentCtrl->getTheme() : nullptr;
		}
	}
};


