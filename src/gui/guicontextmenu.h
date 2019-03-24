#pragma once
#include <vector>
#include "event.h"
#include "str_util.h"
#include "gui.h"
#include "guicolors.h"
#include "guicontextmenu_base.h"
#include "basectrl.h"

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;

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
		size.x = std::max(size.x, entry->width);
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
		parentCtrl->closePopup();
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
	void determineSize(glm::ivec2& prefSize) override {
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
			g->theme = parentCtrl->getTheme();
		}
	}
};


