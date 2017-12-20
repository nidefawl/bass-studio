#pragma once
#include <nanovg.h>
#include "mainctrl.h"
#include "gui.h"
#include "str_util.h"
#include "guicolors.h"
#include "exceptions.h"
#include "mouse.h"
#include "textfield.h"
#include "../host/plugindatabase.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;
class gui_scrollcontainer {
public:
	gui_scrollcontainer() {}
	virtual ~gui_scrollcontainer() {}
	virtual int32_t getContentHeight() = 0;
	virtual int32_t getContentWidth() = 0;
	virtual void scrollOffsetChanged(int dir, float offset) = 0;
};
class gui_scrollbar : public guibase {
	int dir;
	gui_scrollcontainer& ctr;
public:
	float scrollOffset;
	gui_scrollbar(int _dir, float _offset, gui_scrollcontainer& _ctr) : guibase(), dir(_dir), ctr(_ctr), scrollOffset(_offset) {
	}
	virtual void render(NVGcontext* vg) {
		nvgBeginPath(vg);
		nvgRoundedRect(vg, pos.x, pos.y, size.x, size.y, G_RND);
		NVGcolor bg = g_guiColors[COL_BG_DRK];
		nvgFillColor(vg, bg);
		nvgFill(vg);
		int32_t s = 0;
		int32_t cS = 0;
		if (dir == 1) {
			s = size.y;
			cS = ctr.getContentHeight();
		} else {
			s = size.x;
			cS = ctr.getContentWidth();
		}
		float barLen = 0;
		if (cS > 0) {
			barLen = min((float) s, (s / (float) cS) * s);
		}
		float scrollRange = (s-barLen);
		float barOffset = scrollOffset*scrollRange;
		if (cS > 0) {
			float barW = size.x;
			float barH = size.y;
			float barOffsetX = 0;
			float barOffsetY = 0;
			if (dir == 1) {
				barH = barLen;
				barOffsetY = barOffset;
			} else {
				barW = barLen;
				barOffsetX = barOffset;
			}
			int32_t inset = 1;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, pos.x+barOffsetX+inset, pos.y+barOffsetY+inset, barW-inset*2, barH-inset*2, G_RND);


			bool focused = MainCtrl::get()->guiCtrFocused == this->parent || (MainCtrl::get()->guiDragged==NULL&&MainCtrl::get()->guiOver == this);
			if (focused) {
//				nvgStrokeWidth(vg, 1.0f);
//				nvgStrokeColor(vg, g_guiColors[COL_BG_DRK_FOCUSED]);
//				nvgStroke(vg);
				nvgFillColor(vg, g_guiColors[COL_BG_DRK_FOCUSED]);
			} else {
				nvgFillColor(vg, g_guiColors[COL_BG_DRKER]);
			}
			nvgFill(vg);

		}
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
	float startOffset = 0;
	virtual void handleDraggedBegin(MouseEvent& evt) {
		startOffset = scrollOffset;
	}
	void setScrollOffset(float f) {
		float _newOffset = f < 0 ? 0 : f > 1 ? 1 : f;
		scrollOffset = _newOffset;
		ctr.scrollOffsetChanged(dir, scrollOffset);
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
		int32_t s = 0;
		if (dir == 1) {
			s = size.y;
		} else {
			s = size.x;
		}
		if (s>0) {
			int32_t dragPixels = (evt.mousepos-evt.dragStart)[dir];
			setScrollOffset(startOffset + dragPixels/(float)s);
		}

	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
	}
};
#define LIST_ROW_HEIGHT 30
class gui_list_entry : public guibase {
protected:
	int icon = 0;
public:
	gui_list_entry() : guibase() {
	}
	virtual void render(NVGcontext* vg) {
		MainCtrl* ctrl = MainCtrl::get();
		float spacing = INSET_TITLE;
		float x = spacing+spacing;
		if (icon > -1) {
			x += LIST_ROW_HEIGHT;
		}

		if (ctrl->guiFocused == this) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, g_guiColors[COL_BG_DRKER]);
			nvgFill(vg);
		}
		nvgTranslate(vg, pos.x, pos.y);
		if(icon > -1) {
			int32_t extImg = 2;
			int32_t iconW = LIST_ROW_HEIGHT+extImg*2;
			RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
			NVGpaint paintIcon = nvgImagePattern(vg, -extImg, -extImg, iconW, iconW, 0, image.id, 1.0f);
			nvgBeginPath(vg);
			nvgRect(vg, -extImg, -extImg, iconW, iconW);
			nvgFillPaint(vg, paintIcon);
			nvgFill(vg);
		}

		setFont(vg, (int) (LIST_ROW_HEIGHT * 0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, x, LIST_ROW_HEIGHT / 2, StringAsCStr(getText()), NULL);
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
public:
	gui_list() : guictr_base(), scrollbar(1, 0.0f, *this) {
		add(&scrollbar);
	}
	~gui_list() {
		remove(&scrollbar);
		destroyGuis();
	}
	int32_t getContentHeight() override {
		return LIST_ROW_HEIGHT * (int32_t)listGuis.size();
	}
	int32_t getContentWidth() override {
		return size.x;
	}
	void updateVisible() {
		ivec2 cs = getSizeContent();
		float offset = scrollbar.scrollOffset;
		int32_t nEntriesFit = floor(cs.y/(double)LIST_ROW_HEIGHT);
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
//			y += LIST_ROW_HEIGHT;
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
		int scrollW = 20;
		int entryW = cs.x - scrollW;
		scrollbar.size = ivec2(scrollW, cs.y);
		scrollbar.pos = ivec2(cs.x-scrollW, 0);
		int x = 0; int y = 0;
		for (guibase* gui : guis) {
			if (gui == &scrollbar)
				continue;
			gui->pos = ivec2(x, y);
			gui->size = ivec2(entryW, LIST_ROW_HEIGHT);
			y += LIST_ROW_HEIGHT;
		}
		updateVisible();
	}
};
class gui_pluginlist_entry : public gui_list_entry {
public:
	const pluginentry_t entry;
	gui_pluginlist_entry(const pluginentry_t _entry) : gui_list_entry(), entry(_entry) {
		icon = _entry.isSynth ? ICON_SYNTH : ICON_EFFECT;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
		target->pluginEntryDragMove(this, mousepos);
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
		target->pluginEntryDragRelease(this, mousepos);
	}
	String getText() override {
		return entry.name;
	}
};
class guictr_pluginlibrary : public guictr_base {
	const int32_t heightTextField = 30;
	gui_textfield textField;
	gui_list pluginListCtr;
	String curquery = "";
	std::vector<pluginentry_t> pluginsLibList;
public:
	guictr_pluginlibrary() : guictr_base() {
		pluginListCtr.padding = 0;
		add(&textField);
		add(&pluginListCtr);
		textField.setCallback([this](const String& str) {
			curquery = str;
			update();
			return true;
		});
		textField.setPlaceholder("Search");
	}
	~guictr_pluginlibrary() {
		remove(&pluginListCtr);
		remove(&textField);
	}
	void update() {
		MainCtrl *ctrl = MainCtrl::get();
		std::vector<gui_list_entry*> _newList;

		pluginsLibList.clear();
		ctrl->plugindb.query(curquery, pluginsLibList);

		for (pluginentry_t& entry : pluginsLibList) {
			gui_pluginlist_entry* g = new gui_pluginlist_entry(entry);
			_newList.push_back(g);
		}
		pluginListCtr.setList(_newList);
		layout();
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
//					my_printf("clicked on %s %d\n", gui->getClassName().c_str(), (int) h);
					return true;
				}
			}
		}
		return false;
	}
	void layout() {
		ivec2 cs = getSizeContent();
		textField.size = ivec2(cs.x, heightTextField);
		textField.pos = ivec2(0, 0);
		pluginListCtr.pos = ivec2(0, heightTextField);
		pluginListCtr.size = ivec2(cs.x, cs.y-heightTextField);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	virtual void render(NVGcontext* vg) {
		renderBackground(vg);
		if (!setScissorTransform(vg)) {
			return;
		}
		textField.render(vg);
		pluginListCtr.render(vg);
	}
};
