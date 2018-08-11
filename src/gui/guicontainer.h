#pragma once
#include <nanovg.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "basectrl.h"
#include "gui.h"
#include "guicolors.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "str_util.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;

class guictr_base : public guibase {
public:
	int padding = CONTENT_INSET;
	int margin = CTR_SPACING;
	ivec4 snapSides;
	std::vector<guibase*> guis;
	bool sortChildren = false;
public:
	guictr_base(int guiType = 0) : guibase(guiType) {
		setSnapSides(ivec4(0));
	}
	guictr_base(ivec2 _pos, ivec2 _size) : guibase(_pos, _size) {
		setSnapSides(ivec4(0));
	}
	virtual ~guictr_base() {
		assert(guis.empty());
	}
	virtual void destroyGuis() {
		for (guibase* g : guis) {
			g->onRemove();
			g->parent = NULL;
			delete g;
		}
		guis.clear();
	}
	virtual void removeGuis() {
		for (guibase* g : guis) {
			g->onRemove();
			g->parent = NULL;
		}
		guis.clear();
	}
public:
	virtual void onRemove() override {
		removeGuis();
	}
	virtual void onAdded() override {
	}
	virtual void determineSize() override {
	}
	virtual ivec2 paddingTL(int _padding) {
		return ivec2(_padding - margin*snapSides.x, _padding - margin*snapSides.y);
	}
	virtual ivec2 paddingBR(int _padding) {
		return ivec2(_padding - margin*snapSides.z, _padding - margin*snapSides.w);
	}
	//TODO: cache this and remove method
	virtual ivec2 getPosContent() {
		return pos + paddingTL(padding);
	}
	virtual ivec2 getSizeContent() {
		return size - (paddingTL(padding) + paddingBR(padding));
	}
	void renderTitleBarHorizontal(NVGcontext* vg, String text, float textOffsetX);
	void renderFrameBase(NVGcontext* vg);
	void renderFrameOutline(NVGcontext* vg);
	virtual bool setScissorTransformContainer(NVGcontext* vg);
	virtual bool setScissorTransform(NVGcontext* vg) {
		ivec2 posInset = getPosContent();
		ivec2 sizeInset = getSizeContent();
		if (sizeInset.y <= 0 || sizeInset.x <= 0) {
			return false;
		}
		nvgIntersectScissor(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
		nvgTranslate(vg, posInset.x, posInset.y);
		return true;
	}
	void setSnapSides(ivec4 _snapSides) {
		this->snapSides = _snapSides;
	}
	virtual void scissorClip(ivec2& vpos, ivec2& vsize) {
		ivec2 posTL = toParentSpace(vpos);
		ivec2 posBR = toParentSpace(vpos + vsize);
		ivec2 posCnt = getPosContent();
		ivec2 sizeCnt = getSizeContent();
		ivec2 posBRThis = posCnt+sizeCnt;
		vpos.x = max(posTL.x, posCnt.x);
		vpos.y = max(posTL.y, posCnt.y);
		vsize.x = min(posBR.x, posBRThis.x) - vpos.x;
		vsize.y = min(posBR.y, posBRThis.y) - vpos.y;
		if (parent != NULL) {
			parent->scissorClip(vpos, vsize);
		}
		vpos = toContainerSpace(vpos);
	}
	virtual ivec2 toContainerSpace(ivec2 in) {
		return in - getPosContent();
	}
	virtual ivec2 toParentSpace(ivec2 in) {
		return getPosContent() + in;
	}
	virtual ivec2 toScreenSpace(ivec2 in) {
		in += getPosContent();
		if (this->parent != NULL) {
			in = this->parent->toScreenSpace(in);
		}
		return in;
	}
	virtual void add(guibase* gui) {
		auto it = std::find(guis.begin(), guis.end(), gui);
		if (it != guis.end()) {
			throw applogicexception(StringFormat("%s - attempt to add gui twice", StringAsCStr(getClassName())));
		}
		guis.push_back(gui);
		if (sortChildren) {
			std::sort(guis.begin(), guis.end(), [](guibase* a, guibase* b) {
				return a->zOrder > b->zOrder;
			});
		}
		gui->parent = this;
		gui->onAdded();
	}
	bool hasGui(guibase* gui) {
		auto it = std::find(guis.begin(), guis.end(), gui);
		return it != guis.end();
	}
	virtual void remove(guibase* gui) {
		auto it = std::find(guis.begin(), guis.end(), gui);
		if (it == guis.end()) {
			if (gui->parent == nullptr)
				return;
			throw applogicexception(StringFormat("%s - attempt to remove non-present element", StringAsCStr(getClassName())));
		}
		gui->onRemove();
		guis.erase(it);
		gui->parent = NULL;
	}
	virtual void addUNCHECKED(guibase* gui) {
		auto it = std::find(guis.begin(), guis.end(), gui);
		if (it != guis.end()) {
			return;
		}
		guis.push_back(gui);
		if (sortChildren) {
			std::sort(guis.begin(), guis.end(), [](guibase* a, guibase* b) {
				return a->zOrder > b->zOrder;
			});
		}
		gui->parent = this;
	}
	virtual void removeUNCHECKED(guibase* gui) {
		auto it = std::find(guis.begin(), guis.end(), gui);
		if (it == guis.end()) {
			return;
		}
//		gui->onRemove();
		guis.erase(it);
		gui->parent = NULL;
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
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
	static void drawBackground(NVGcontext* vg, ivec2 posInset, ivec2 sizeInset, int margin, bool focused = false, bool drawInset = true) {
		static const ivec2 borderThickness(CTR_SPACING-2);
		posInset -= ivec2(margin);
		sizeInset += ivec2(margin) * 2;
		if (sizeInset.y > 0 && sizeInset.x > 0) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y, 4);
			NVGcolor bg = g_guiColors[COL_BG_DRK];
			if (focused) {
				bg = g_guiColors[COL_BG_DRK_FOCUSED];
			}
			nvgFillColor(vg, bg);
			nvgFill(vg);
			posInset += borderThickness;
			sizeInset -= borderThickness * 2;
			if (sizeInset.y > 0 && sizeInset.x > 0 && drawInset) {
				nvgBeginPath(vg);
				nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
				nvgFillColor(vg, g_guiColors[COL_BG_BRT]);
				nvgFill(vg);
			}
		}
	}
	virtual void renderBackground(NVGcontext* vg) {
		bool focused = AppCtrl::get()->isCtrOrChildFocused(this);
		drawBackground(vg, getPosContent(), getSizeContent(), margin, focused, true);
	}
	static void drawInsetBackground(NVGcontext* vg, ivec2 posInset, ivec2 sizeInset) {
		if (sizeInset.y > 0 && sizeInset.x > 0) {
			nvgBeginPath(vg);
			nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
			nvgFillColor(vg, g_guiColors[COL_BG_BRT]);
			nvgFill(vg);
		}
	}
	virtual void onIdle() {
		for (guibase* gui : guis) {
			gui->onIdle();
		}
	}
	virtual void prerender(NVGcontext* vg) {
		for (guibase* gui : guis) {
			gui->prerender(vg);
		}
	}
	virtual void onTick(AppCtrl* ctrl) {
	}
	virtual guibase* getFocusedContainer() {
		if (this->parent != NULL){
			return this->parent->getFocusedContainer();
		}
		return this;
	}
#if RENDER_DBG_BRD
	void renderDebug(NVGcontext* vg, NVGcolor color) {
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, size.y);
		nvgFillColor(vg, color);
		nvgFill(vg);
		ivec2 posInset = getPosContent();
		ivec2 sizeInset = getSizeContent();
		nvgBeginPath(vg);
		nvgRect(vg, posInset.x, posInset.y, sizeInset.x, sizeInset.y);
		nvgFillColor(vg, color);
		nvgFill(vg);
	}
#endif
};
class Splitter : public guictr_base {
public:
	int type;
	float scale;
	float min, max;
	Splitter(int _type, float _scale)
	: guictr_base(),
	  type(_type),
	  scale(_scale)
	{
		min = 0;
		max = 1;
		padding = 0;
	}
	void setMinMax(float _min, float _max) {
		this->min = _min;
		this->max = _max;
	}
	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos) && evt.type <= MouseHitType::MOUSE_RIGHT) {
			evt.requestFocus(this);
			evt.requestCursor(type == 0 ? CURSOR_RESIZE_V : CURSOR_RESIZE_H);
			return true;
		}
		return false;
	}
	int32_t leftOrTop(int32_t wh) {
		return round(wh*scale);
	}
	int32_t rightOrBottom(int32_t wh) {
		return wh-leftOrTop(wh);
	}
	virtual void handleDraggedBegin(MouseEvent& evt) {
	}
	virtual void handleDraggedMove(MouseEvent& evt) {
		ivec2 windowSize = AppCtrl::get()->m_size;
		float sc = type == 0  ? (evt.mousepos.y/(float)windowSize.y) : (evt.mousepos.x/(float)windowSize.x);
		this->scale = (sc < min ? min : sc > max ? max : sc);
		AppCtrl::get()->relayout(windowSize.x, windowSize.y);
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
	}
	virtual bool isStaticContainer() {
		return true;
	}
};
