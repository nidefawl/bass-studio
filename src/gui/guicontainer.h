#pragma once
#include <nanovg_min.h>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "seq_math.h"
#include "str_util.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "guiconstant.h"
#include "gui.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;
class BaseCtrl;
struct guitheme_t;
class guictr_base : public guibase {
public:
	int padding = CONTENT_INSET;
	int margin = CTR_SPACING;
	ivec4 snapSides;
	std::vector<guibase*> guis;
	bool sortChildren = false;
public:
	guictr_base() : guibase() {
		setSnapSides(ivec4(0));
		setBackgroundRendered(false);
		setBackgroundRenderedInset(true);
	}
	virtual ~guictr_base() {
		assert(guis.empty());
	}
	virtual void destroyGuis() {
		for (guibase* g : guis) {
			g->onRemove();
			g->setParent(nullptr);
			//g->setControl(nullptr);
			delete g;
		}
		guis.clear();
	}
	virtual void removeGuis() {
		for (guibase* g : guis) {
			g->onRemove();
			g->setParent(nullptr);
			//g->setControl(nullptr);
		}
		guis.clear();
	}

	virtual void setControl(BaseCtrl* parentCtrl) override;
	virtual void setParent(guibase* parent) override;
public:
	static void drawBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset, int margin, bool focused = false, bool drawInset = true);
	static void drawInsetBackground(NVGcontext* vg, const guitheme_t* theme, ivec2 posInset, ivec2 sizeInset);

	virtual void onRemove() override;
	virtual void onAdded() override;
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
	void renderTitleBar(NVGcontext* vg, String text, GuiConstant::constant_t& constantHeight, float textOffsetX, int flags, bool isHorizontalTitle);
	void renderFrameBase(NVGcontext* vg);
	void renderFrameOutline(NVGcontext* vg);
	virtual void renderBackground(NVGcontext* vg);
	virtual void render(NVGcontext* vg);
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
		gui->setParent(this);
		gui->setControl(getControl());
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
		gui->setParent(nullptr);
		//gui->setControl(nullptr);
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
		gui->setParent(this);
		gui->setControl(getControl());
	}
	virtual void removeUNCHECKED(guibase* gui) {
		auto it = std::find(guis.begin(), guis.end(), gui);
		if (it == guis.end()) {
			return;
		}
//		gui->onRemove();
		guis.erase(it);
		gui->setParent(nullptr);
		//gui->setControl(nullptr);
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
		for (guibase* gui : guis) {
			gui->onTick(ctrl);
		}
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
class guictr_tabbed : public guictr_base {

	struct tabbed_entry;
	std::vector<tabbed_entry*> entries;
	tabbed_entry* activeEntry = nullptr;
	ivec2 sizeContentTab;
public:
	guictr_tabbed() : guictr_base() {

	}
	int32_t getNumEntries();
	void setActiveEntry(int32_t idx);
	void addEntry(guictr_base* ctr, String title);
	virtual void buttonClicked(guibase* button) override;
	virtual ~guictr_tabbed();
	void layout() override;
	void render(NVGcontext* vg);
};

