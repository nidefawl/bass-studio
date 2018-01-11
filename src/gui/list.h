#pragma once
#include <nanovg.h>
#include "gui.h"
#include "guicontainer.h"
#include "scrollbar.h"

class gui_list_entry : public guibase {
protected:
	int icon = 0;
public:
	gui_list_entry() : guibase() {
	}
	virtual void render(NVGcontext* vg) {
		MainCtrl* ctrl = MainCtrl::get();
		float spacing = INSET_TITLE;
		float x = spacing;
		float rowHeight = size.y;
		if (icon > -1) {
			x += rowHeight+spacing;
		}

		if (ctrl->isCtrOrChildFocused(this)) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_DRKER]);
			nvgFill(vg);
		}
		nvgTranslate(vg, pos.x, pos.y);
		if(icon > -1) {
			int32_t extImg = 2;
			int32_t iconW = rowHeight+extImg*2;
			RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
			NVGpaint paintIcon = nvgImagePattern(vg, -extImg, -extImg, iconW, iconW, 0, image.id, 1.0f);
			nvgBeginPath(vg);
			nvgRect(vg, -extImg, -extImg, iconW, iconW);
			nvgFillPaint(vg, paintIcon);
			nvgFill(vg);
		}

		setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
		nvgTranslate(vg, -pos.x, -pos.y);
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	virtual void handleDraggedBegin(MouseEvent& evt) {
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
		MainCtrl::get()->objectDragMove(this, evt);
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		MainCtrl::get()->objectDragRelease(this, evt);
	}
	virtual void dragMoveOn(guibase* target, ivec2 mousepos) = 0;
	virtual void dragReleaseOn(guibase* target, ivec2 mousepos) = 0;
	virtual String getText() = 0;
	bool isDragMoveable() {
		return true;
	}
};
class gui_list : public guictr_base, public gui_scrollcontainer {
	gui_scrollbar scrollbar;
	std::vector<gui_list_entry*> listGuis;
	uint32_t first = 0;
	uint32_t last = 0;
	int rowHeight = 30;
public:
	gui_list(int _rowHeight = 30) : guictr_base(), scrollbar(1, 0.0f, *this), rowHeight(_rowHeight) {
		add(&scrollbar);
	}
	~gui_list() {
		remove(&scrollbar);
		destroyGuis();
	}
	void setRowHeight(int h) {
		rowHeight = h;
	}
	ivec2 getScrollTotalSize() override {
		ivec2 cs = getSizeContent();
		cs.y = rowHeight * (int32_t)listGuis.size();
		return cs;
	}
	ivec2 getScrollViewSize() override {
		return getSizeContent();
	}
	void updateVisible() {
		ivec2 cs = getSizeContent();
		float offset = scrollbar.scrollOffset;
		int32_t nEntriesFit = floor(cs.y/(double)rowHeight);
		int32_t nEntries = max(0, (int32_t)listGuis.size()-nEntriesFit);
		first = (uint32_t)max(0, (int32_t) floor(offset * nEntries));
		if (listGuis.size() == 0) {
			first = last = 0;
		} else {
			last = first + (int32_t) nEntriesFit+1;
			first = min((uint32_t)(listGuis.size()-1), first);
			last = min((uint32_t)listGuis.size(), last);
		}
	}

	void scrollOffsetChanged(int dir, float offset) {
		updateVisible();
	}

	virtual void render(NVGcontext* vg) {
//		guictr_base::renderBackground(vg);
		if (!setScissorTransform(vg)) {
			return;
		}

		nvgSave(vg);
		if (first < last) {
			gui_list_entry* g = listGuis[first];
			nvgTranslate(vg, 0, -g->top());
		}
		for (int32_t idx = first; idx < last; idx++) {
			listGuis[idx]->render(vg);
		}
		nvgRestore(vg);
//		int x = 0; int y = 0;
//		nvgBeginPath(vg);
//		for (int32_t idx = first; idx < last; idx++) {
//			if (y > 0) {
//				nvgMoveTo(vg, x, y);
//				nvgLineTo(vg, x+cs.x, y);
//			}
//			y += rowHeight;
//		}
//		nvgStrokeWidth(vg, 1.0f);
//		nvgStrokeColor(vg, G_BLACK);
//		nvgStroke(vg);

		scrollbar.render(vg);
		nvgResetScissor(vg);
		nvgResetTransform(vg);
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			if (scrollbar.mouseHitTest(localMouse, evt)) {
//				my_printf("clicked on %s %d\n", scrollbar.getClassName().c_str(), (int) h);
				return true;
			}
			ivec2 localMouseOffset = localMouse;
			if (first < last) {
				gui_list_entry* g = listGuis[first];
				localMouseOffset.y += g->top();
			}
			for (int32_t idx = first; idx < last; idx++) {
				if (listGuis[idx]->mouseHitTest(localMouseOffset, evt)) {
//					my_printf("clicked on %s %s %d\n", listGuis[idx]->getClassName().c_str(), listGuis[idx]->getText().c_str(), (int) h);
					return true;
				}
			}
			evt.requestFocus(this);
//			my_printf("clicked on %s %d\n", getClassName().c_str(), (int) h);
			return true;
		}
		return false;
	}
	void setList(std::vector<gui_list_entry*> _newList) {
		for (gui_list_entry* g : listGuis) {
			remove(g);
			delete g;
		}
		listGuis = _newList;
		for (gui_list_entry* g : listGuis) {
			add(g);
		}
		layout();
	}
	void layout() {
		ivec2 cs = getSizeContent();
		int scrollW = gui_scrollbar::defaultW;
		int entryW = cs.x - scrollW;
		scrollbar.size = ivec2(scrollW, cs.y);
		scrollbar.pos = ivec2(cs.x-scrollW, 0);
		int x = 0; int y = 0;
		for (guibase* gui : guis) {
			if (gui == &scrollbar)
				continue;
			gui->pos = ivec2(x, y);
			gui->size = ivec2(entryW, rowHeight);
			y += rowHeight;
		}
		for (guibase* gui : guis) {
			gui->layout();
		}
		updateVisible();
	}
	virtual bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
		return scrollbar.handleMouseScroll(evt, xoffset, yoffset);
	}
};
