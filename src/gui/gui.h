#pragma once
#include <glm/vec2.hpp>
#include <nanovg.h>
#include <vector>
#include <algorithm>
#include <typeinfo>

#include "str_util.h"
#include "event.h"
#include "math.h"
#include "guicolors.h"
#include "logging.h"
#include "renderresources.h"
#include "mainctrl.h"

using glm::vec2;
using glm::ivec2;

struct NVGcontext;
class guitrack_editor;
class guiplugin;
class guictr_base;
class gui_pluginlist_entry;

extern int allocCount;
extern std::vector<guibase*> g_guis;
void initColor();
void setFont(NVGcontext* vg, float size, NVGcolor color, int alignment);
void renderText(NVGcontext* ctx, float x, float y, float maxWidth, const char* string);
void renderDashedLineFrame(NVGcontext* vg, float x, float y, float w, float h, float thickness);
void drawAttachedBackground(NVGcontext* vg, ivec2 posInset, ivec2 sizeInset, int margin);

void drawPlaySymbol(NVGcontext* vg, ivec2& pos, ivec2& size, NVGcolor& color, int drawParm, int drawParm2);
void drawStopSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, NVGcolor& color, int drawParm, int drawParm2);
void drawTextureSymbol(NVGcontext* vg, ivec2& pos, ivec2& size, NVGcolor& color, int drawParm, int drawParm2);
void drawTri(NVGcontext* vg, float xTop, float yTop, float h, const int dir, const NVGcolor& color, const NVGcolor& strokeColor, float strokeWidth);
inline float calcInset(float desiredInset, float size) {
	return min(desiredInset, max(0.f, (size-4.0f)/2.0f));
}
class guibase {
public:
	ivec2 pos;
	ivec2 size;
	guibase* parent = NULL;
	int zOrder = 0;
	int id;
	guibase() {
		id = allocCount;
		allocCount++;
		g_guis.push_back(this);
	}
	guibase(ivec2 _pos, ivec2 _size) : guibase() {
		this->pos = _pos;
		this->size = _size;
	}
	String getClassName() {
		return typeName(*this);
	}
	virtual ~guibase() {
		MainCtrl* ctrl = MainCtrl::get();
		ctrl->onGuiRemoved(this); // TODO: don't call this from here, feels nasty
		allocCount--;
		auto it = std::find(g_guis.begin(), g_guis.end(), this);
		g_guis.erase(it);
	}
	guibase(const guibase&) = default; guibase& operator=(const guibase&) = default;
	guibase(guibase&&) = default; guibase& operator=(guibase&&) = default;

	bool contains(ivec2 mpos) {
		return mpos.x >= pos.x &&
			mpos.y >= pos.y &&
			mpos.x < pos.x + size.x &&
			mpos.y < pos.y + size.y;
	}
	ivec2 getRightTop() {
		return pos + ivec2(size.x, 0);
	}
	int right() {
		return pos.x+size.x;
	}
	int top() {
		return pos.y;
	}
	int bottom() {
		return pos.y+size.y;
	}
	int left() {
		return pos.x;
	}
	void setZOrder(int _zOrder) {
		zOrder = _zOrder;
	}
	virtual void render(NVGcontext* vg) {

	}
	virtual void renderDragged(NVGcontext* vg, ivec2 mousepos) {

	}
	virtual void layout() {
	}
	virtual void onRemove() {
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
		return false;
	}
	guibase* getTopParent() {
		guibase* parentGui = parent;
		while (parentGui != NULL) {
			parentGui = parentGui->parent;
		}
		return parentGui;
	}
	virtual void handleRightClick(MouseEvent& evt) {
	}
	virtual void handleDraggedBegin(MouseEvent& evt) {
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
	}
	virtual bool handleKeyInput(KeyEvent& kevt) {
		return false;
	}
	virtual bool handleCharInput(unsigned int codepoint) {
		return false;
	}
	virtual void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {

	}
	virtual void trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {

	}
	virtual void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {

	}
	virtual bool clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos) {
		return false;
	}
	virtual bool clipDropMove(dragdrop_midifile& clip, ivec2 mousepos) {
		return false;
	}
	virtual bool clipDropFinal(dragdrop_midifile& clip, ivec2 mousepos) {
		return false;
	}
	virtual void pluginDragMove(guiplugin* g, ivec2 mousepos) {
	}
	virtual void pluginDragRelease(guiplugin* g, ivec2 mousepos) {
	}
	virtual void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) {
	}
	virtual void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) {
	}
	virtual void dragBeginOn(guibase* target, ivec2 mousepos) {
	}
	virtual void dragMoveOn(guibase* target, ivec2 mousepos) {
	}
	virtual void dragReleaseOn(guibase* target, ivec2 mousepos) {
	}
	virtual void buttonClicked(guibase* button) {
	}
	/*
	 * determines if drag operations should focus containers
	 * when hovering target containers for short periods
	 */
	virtual bool isDragMoveable() {
		return false;
	}
	virtual bool setScissorTransform(NVGcontext* vg) {
		ivec2 posInset = pos;
		ivec2 sizeInset = size;
		if (sizeInset.y <= 0 || sizeInset.x <= 0) {
			return false;
		}
		nvgIntersectScissor(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
		nvgTranslate(vg, posInset.x, posInset.y);
		return true;
	}
	virtual ivec2 toContainerSpace(ivec2 in) {
		return in - this->pos;
	}
	void getHierachy(std::vector<guibase*>& stack) {
		guibase* p = this->parent;
		while (p != NULL) {
			stack.push_back(p);
			p = p->parent;
		}
	}
	virtual void onChildLayoutChanged(guibase* g) {
		if (this->parent != NULL) {
			this->parent->onChildLayoutChanged(this);
		}
	}
	virtual guibase* getFocusedControl() {
		return this;
	}
    virtual bool focusEvent(bool focused) {
    	return true;
    }
	virtual guibase* getFocusedContainer() {
		if (this->parent != NULL){
			return this->parent->getFocusedContainer();
		}
		return NULL;
	}
	void renderWidgetBorder(NVGcontext* vg) {
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgStrokeColor(vg, g_guiColors[COL_GUI_STROKE]);
		nvgStrokeWidth(vg, 3);
		nvgStroke(vg);
		nvgFillColor(vg, g_guiColors[COL_BG_DRK]);
		nvgFill(vg);
	}
	virtual ivec2 toScreenSpace(ivec2 in) {
		in += this->pos;
		if (this->parent != NULL) {
			in = this->parent->toScreenSpace(in);
		}
		return in;
	}
	virtual bool isStaticContainer() {
		return true;
	}
protected:

};

