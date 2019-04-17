#pragma once
#include "str_util.h"
#include "color_util.h"
#include "guicontainer.h"
#include "button.h"
#include "track.h"
#include "basectrl.h"
#include "table.h"
#include "../host/mainctrl.h"
#include "math/vec.h"
#include "guiplugin.h"

class vstplugin;
class effectbase;
struct audio_stage_t;

class guictr_test : public guictr_base {
public:
	guictr_test() : guictr_base() {
		setBackgroundRendered(true);

	}
	~guictr_test() {
//		for (auto it = guis.begin(); it != guis.end(); it++) {
//			delete (*it);
//		}
		guis.clear();
	}
	void render(NVGcontext* vg) {
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
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
	String message;
	guiplaceholder() : guibase() {

	}
	~guiplaceholder() {

	}
	void render(NVGcontext* vg) {
		nvgBeginPath(vg);
//		float fRnd = theme->getFloat(GuiConstant::CONST_ROUND);
//		nvgRoundedRect(vg, pos.x, pos.y, size.x, size.y, fRnd);
		NVGcolor c;
		if (this == parentCtrl->guiOver) {
			c = theme->getFrameColorHighlight();
		}
		else {
			c = theme->getFrameColorOutline();
		}
		nvgFillColor(vg, theme->getFrameColorBase());
		nvgFill(vg);
		setFont(vg, 18, G_WHITE, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
		nvgText(vg, pos.x + size.x/2.0f, pos.y + size.y/2.0f, StringAsCStr(message), NULL);
	}
	void determineSize(ivec2& prefSize) override {
		size.x = math::max(100, size.y*3/5);
	}
};
class guictr_dragged_plugins : public guictr_base {
	const int HEIGHT_ENTRY = 20;
public:
	std::vector<effectbase*> effects;
	audio_stage_t* trackImpl = nullptr;
	Table::tbl table;
	guictr_dragged_plugins() : guictr_base() {
		pos = {0, 0};
	}
	~guictr_dragged_plugins() {
	}
	void layout() override {
	}
	bool isDragMoveable() {
		return true;
	}
	virtual audio_stage_t* getTrackLink() {
		return trackImpl;
	}
	void renderDragged(NVGcontext* vg, ivec2 mousepos, ivec2 dragOffset) override;
	void setStrings(std::vector<String>& list);
	void handleDraggedRelease(MouseEvent& evt);
	void handleDraggedMove(MouseEvent& evt);
	void dragMoveOn(guibase* target, ivec2 mousepos);
	void dragReleaseOn(guibase* target, ivec2 mousepos);
};
class guictr_plugins : public guictr_base {
public:
	guiplaceholder placeholder;
	guictr_dragged_plugins dragged;
	track_t* track = nullptr;
	audio_stage_t* stage = nullptr;
	int scrolloffset = 0;
	bool isDefaultPluginCtr = true;
public:
	guictr_plugins() : guictr_base() {
		setBackgroundRendered(true);
		dragged.setParent(this);
	}
	~guictr_plugins() {
		removeEntry(guis, &placeholder);
		guis.clear();
	}
	virtual void setControl(BaseCtrl* parentCtrl) override {
		guibase::setControl(parentCtrl);
		placeholder.setControl(parentCtrl);
		dragged.setControl(parentCtrl);
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
	virtual ivec2 toParentSpace(ivec2 in) {
		in.x -= scrolloffset;
		ivec2 offsetPos = getPosContent() + in;
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
		nvgLineCap(vg, NVGlineCap::NVG_ROUND);
		nvgBeginPath(vg);
		nvgMoveTo(vg, posHL.x, 4);
		nvgLineTo(vg, posHL.x, getSizeContent().y - 4);
		nvgStrokeColor(vg, G_MOVE_HIGHLIGHT);
		nvgStrokeWidth(vg, 4.0);
		nvgStroke(vg);
		nvgLineCap(vg, NVGlineCap::NVG_BUTT);
	}
	void render(NVGcontext* vg);
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	bool handleKeyInput(KeyEvent& kevt) override;
	void layout() override;
	int slotFromCoord(ivec2 _pos);
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
			return 1;
		}
		return last->pos.x + last->size.x + 50;
	}

	virtual void onAdded() override;
	virtual void onTick(AppCtrl* ctrl) override;
	void pluginDragMove(guiplugin* g, ivec2 mousepos) override;
	void pluginDragRelease(guiplugin* g, ivec2 mousepos) override;
	void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override;
	void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override;
	void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override;
	void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override;
	void showTrack(audio_stage_t* track);
	void hideTrack(audio_stage_t* track);
	void onSelected(MouseEvent& evt, guiplugin* plugin);
	void relayout();
	void addGui(effectbase* plugin);
	void onChildLayoutChanged(guibase* g) override;
	virtual void determineSize(ivec2& prefSize) override;
	virtual guibase* getDraggedControl() override;
	void getEffects(std::vector<effectbase*>& out);
	virtual bool isSelected() override;
};
class guictr_pluginview : public guictr_base {
public:
	int lastscrolloffset = 0;
	guictr_plugins* ctr_plugins;
	guictr_pluginview(guictr_plugins* _plugins) : guictr_base() {
		this->ctr_plugins = _plugins;
		setCanMouseHit(true);
	}
	~guictr_pluginview() {
	}
	vec2 getScale() {
		ivec2 cs = this->getSizeContent();
		ivec2 csp = ctr_plugins->getSizeContent();
		int32_t w = ctr_plugins->getTotalWidth();
		float sc = math::max(1.0f, csp.x / (float) w);
		return vec2((cs.x / (double)csp.x)*sc, cs.y / (double)csp.y);
	}



	void render(NVGcontext* vg);
	void handleDraggedBegin(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			MainCtrl::get()->showPluginView();
			lastscrolloffset = ctr_plugins->scrolloffset;
		}
	}
	float getMinScale() {
		ivec2 cs = this->getSizeContent();
		ivec2 csp = ctr_plugins->getSizeContent();
		if (cs.x > 0 && cs.y > 0 && csp.x > 0 && csp.y > 0) {
			int32_t w = ctr_plugins->getTotalWidth();
			return math::min((cs.x / (float) math::max(csp.x,w)), cs.y / (float) csp.y);
		}
		return 1.0f;
	}
	void handleDraggedMove(MouseEvent& evt) {
		if (evt.guiDragged == this) {
			ivec2 move = evt.mousepos - evt.dragStart;
			ctr_plugins->setScrolloffset(lastscrolloffset + (int)(move.x*(1.0 / getMinScale())));
		}
	}
	void layout() {
	}
};

