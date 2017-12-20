#pragma once
#include <glm/vec2.hpp>
#include "str_util.h"
#include "color_util.h"
#include "guicontainer.h"
#include "button.h"
using glm::vec2;
using glm::ivec2;

class vstplugin;

class guictr_test : public guictr_base {
public:
	guictr_test() : guictr_base() {

	}
	~guictr_test() {
//		for (auto it = guis.begin(); it != guis.end(); it++) {
//			delete (*it);
//		}
		guis.clear();
	}
	void render(NVGcontext* vg) {
		renderBackground(vg);
		if (!setScissorTransform(vg)) {
			return;
		}
		for (auto it = guis.rbegin(); it != guis.rend(); it++) {
			guibase *gui = *it;
			gui->render(vg);
		}
		nvgResetScissor(vg);
		nvgResetTransform(vg);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
		}
		return false;
	}
	void layout() {
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	void handleDraggedMove(MouseEvent& evt) {
		ivec2& guiPos = evt.guiDragged->pos;
		guiPos = evt.mousepos + evt.dragOffset;
	}
	void handleDraggedRelease(MouseEvent& evt) {
	}
};
class guiplaceholder : public guibase {
public:
	guiplaceholder() : guibase() {

	}
	~guiplaceholder() {

	}
	void render(NVGcontext* vg) {
		nvgBeginPath(vg);
//		nvgRoundedRect(vg, pos.x, pos.y, size.x, size.y, G_RND);
		NVGcolor c;
		if (this == MainCtrl::get()->guiOver) {
			c = GUI_COLOR(G_S3);
		}
		else {
			c = GUI_COLOR(G_S1);
		}
		nvgFillColor(vg, GUI_COLOR(G_S2));
		nvgFill(vg);
		setFont(vg, 18, G_WHITE, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
		nvgText(vg, pos.x + size.x/2.0f, pos.y + size.y/2.0f, "Drop Instruments here", NULL);
	}
};
class guictr_plugins : public guictr_base {
public:
	int scrolloffset = 0;
	int32_t highlightSlot = -1;
	track_t* track = NULL;
	guiplaceholder placeholder;
	guictr_plugins() : guictr_base() {

	}
	~guictr_plugins() {
		removeEntry(guis, &placeholder);
		guis.clear();
	}
	void setScrolloffset(int offset) {
		if (offset < 0) {
			offset = 0;
		}
		int w = getSizeContent().x;
		int totalWidth = getTotalWidth();
		if (w >= totalWidth) {
			offset = 0;
		}
		else if (offset >= totalWidth - w) {
			offset = totalWidth - w;
		}
		this->scrolloffset = offset;
		if (this->track) {
			this->track->scrolloffset = offset;
		}
	}
	virtual ivec2 toContainerSpace(ivec2 in) {
		ivec2 offsetPos = in - getPosContent();
		offsetPos.x += scrolloffset;
		return offsetPos;
	}
	virtual ivec2 toScreenSpace(ivec2 in) {
		in += getPosContent();
		in.x -= scrolloffset;
		if (this->parent != NULL) {
			in = this->parent->toScreenSpace(in);
		}
		return in;
	}
	void verticalLineAt(NVGcontext* vg, ivec2 posHL) {
		nvgBeginPath(vg);
		nvgMoveTo(vg, posHL.x, 4);
		nvgLineTo(vg, posHL.x, getSizeContent().y - 4);
		nvgLineCap(vg, NVG_ROUND);
		nvgStrokeColor(vg, G_MOVE_HIGHLIGHT);
		nvgStrokeWidth(vg, 4.0);
		nvgStroke(vg);
	}
	void render(NVGcontext* vg) {
		renderBackground(vg);
		if (!setScissorTransform(vg)) {
			return;
		}
		int32_t slot = 0;
		guibase * lastGui = NULL;
		nvgTranslate(vg, -scrolloffset, 0);
		for (guibase* gui : guis) {
			if (highlightSlot == slot) {
				ivec2 posHL(gui->pos.x - 4, 0);
				verticalLineAt(vg, posHL);
				nvgTranslate(vg, 8, 0);
			}
			nvgSave(vg);
			gui->render(vg);
			nvgRestore(vg);
			lastGui = gui;
			slot++;
		}
		if (highlightSlot == slot) {
			ivec2 posHL((lastGui ? (lastGui->pos.x + lastGui->size.x + 8) : -4), 0);
			verticalLineAt(vg, posHL);
		}
		nvgResetScissor(vg);
		nvgResetTransform(vg);
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
				evt.requestFocus(this);
				return true;
			}
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
		}
		return false;
	}
	void layout() {
		ivec2 sizeInset = getSizeContent();
		int32_t guiH = sizeInset.y - margin;
		int32_t inset = margin / 2;
		ivec2 gPos(inset * 3, 0);
		for (guibase* gui : guis) {
			gui->pos = gPos;
			gui->size = ivec2(guiH);
			gui->pos.y = inset;
			gPos.x += gui->size.x + 16;
			gui->layout();
		}
	}
	int slotFromCoord(ivec2 _pos) {
		int slot = 0;
		for (guibase* gui : guis) {
			if (_pos.x+scrolloffset < gui->pos.x + gui->size.x / 2) {
				break;
			}
			slot++;
		}
		return slot;
	}
	int slotFromChild(guibase* child) {
		int slot = 0;
		for (guibase* gui : guis) {
			if (gui == child) {
				return slot;
			}
			slot++;
		}
		return -1;
	}
	int getTotalWidth() {
		guibase* last = guis.empty() ? NULL : guis.back();
		if (!last) {
			return getSizeContent().x;
		}
		return last->pos.x + last->size.x + 50;
	}
	virtual void onTick(MainCtrl* ctrl) {
#define SCROLL_START_X 30

		if (MainCtrl::get()->guiDragged != NULL && MainCtrl::get()->guiDragged->parent == this) {
			if (ctrl->m_mousePos.x < SCROLL_START_X && scrolloffset > 0) {
				setScrolloffset(scrolloffset - (int)((TIMER_MS / 50.0) * 40));
			}
			if (ctrl->m_mousePos.x > getSizeContent().x - SCROLL_START_X && scrolloffset < getTotalWidth() - getSizeContent().x) {
				setScrolloffset(scrolloffset + (int)((TIMER_MS / 50.0) * 40));
			}
			MainCtrl::get()->requestRedraw();
		}
	}
	void pluginDragMove(guiplugin* g, ivec2 mousepos) override;
	void pluginDragRelease(guiplugin* g, ivec2 mousepos) override;
	void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override;
	void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override;
	void showTrack(track_t* track);
	void addGui(vstplugin* plugin);
};
class guictr_pluginview : public guictr_base {
public:
	int lastscrolloffset = 0;
	guictr_plugins* ctr_plugins;
	guictr_pluginview(guictr_plugins* _plugins) : guictr_base() {
		this->ctr_plugins = _plugins;
	}
	~guictr_pluginview() {
	}
	vec2 getScale() {
		ivec2 cs = this->getSizeContent();
		ivec2 csp = ctr_plugins->getSizeContent();
		return vec2(cs.x / (double)ctr_plugins->getTotalWidth(), cs.y / (double)csp.y);
	}



	void render(NVGcontext* vg) {
		ivec2 borderThickness(4);
		ivec2 cp = this->getPosContent();
		ivec2 cs = this->getSizeContent();
		if (MainCtrl::get()->isPluginViewVisible()) {
			drawAttachedBackground(vg, cp, cs, margin);
		} else {
			drawBackground(vg, cp, cs, margin, false);
		}

		ivec2 csp = ctr_plugins->getSizeContent();
		vec2 scale = getScale();
		float minScale = min(scale.x, scale.y);
		if (minScale >= 0 && scale.y) {
			nvgSave(vg);
			if (setScissorTransform(vg)) {
				nvgScale(vg, minScale, scale.y);
				for (guibase* gui : ctr_plugins->guis) {
					nvgSave(vg);
					gui->render(vg);
					nvgRestore(vg);
				}
			}
			nvgRestore(vg);
			nvgBeginPath(vg);
			nvgRect(vg, cp.x + ctr_plugins->scrolloffset*minScale, cp.y, csp.x*minScale, cs.y);
			nvgStrokeWidth(vg, 3);
			nvgStrokeColor(vg, G_BLACK);
			nvgStroke(vg);

		}
	}
	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			MainCtrl::get()->showPluginView();
			lastscrolloffset = ctr_plugins->scrolloffset;
		}
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			ivec2 move = evt.mousepos - evt.dragStart;
			vec2 scale = getScale();
			float minScale = min(scale.x, scale.y);
			ctr_plugins->setScrolloffset(lastscrolloffset + (int)(move.x*(1.0 / minScale)));
		}
	}
	void layout() {
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			evt.requestFocus(this);
			return true;
		}
		return false;
	}
};

