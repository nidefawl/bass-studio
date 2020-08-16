#pragma once
#include <nanovg_min.h>
#include "math/vec.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "guiconstant.h"
#include "gui.h"
#include "assert_dbg.h"

class BaseCtrl;
struct guitheme_t;
class guictr_base : public guibase {
public:
	int padding = CONTENT_INSET;
	int margin = CTR_SPACING;
	ivec4 snapSides{ 0, 0, 0, 0 };
	std::vector<guibase*> guis;
	bool sortChildren = false;
public:
	guictr_base() : guibase() {
		setBackgroundRendered(false);
		setBackgroundRenderedInset(true);
	}
	virtual ~guictr_base() {
		dbgassert(guis.empty());
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
	virtual void determineSize(ivec2& prefSize) override {
	}
	virtual ivec2 paddingTL(int _padding) const {
		return ivec2(_padding - margin*snapSides.x, _padding - margin*snapSides.y);
	}
	virtual ivec2 paddingBR(int _padding) const {
		return ivec2(_padding - margin*snapSides.z, _padding - margin*snapSides.w);
	}
	virtual ivec2 getPosContent() const {
		return pos + paddingTL(padding);
	}
	virtual ivec2 getSizeContent() {
		return size - (paddingTL(padding) + paddingBR(padding));
	}
	ivec2 getPadding() {
		return (paddingTL(padding) + paddingBR(padding));
	}
	void renderTitleBar(NVGcontext* vg, const ivec2& sizeContent, String text, GuiConstant::constant_t& constantHeight, float textOffsetX, int flags, bool isHorizontalTitle);
	void renderFrameBase(NVGcontext* vg);
	void renderFrameOutline(NVGcontext* vg);
	virtual void renderBackground(NVGcontext* vg);
	virtual void renderContainerLabel(NVGcontext* vg);
	virtual void render(NVGcontext* vg);
	virtual bool setScissorTransformContainer(NVGcontext* vg);
	virtual bool setScissorTransform(NVGcontext* vg);
	void setSnapSides(ivec4 _snapSides) {
		this->snapSides = _snapSides;
	}
	virtual void scissorClip(ivec2& vpos, ivec2& vsize);
	virtual ivec2 toContainerSpace(ivec2 in) {
		return in - getPosContent();
	}
	virtual ivec2 toParentSpace(ivec2 in) {
		return getPosContent() + in;
	}
	virtual ivec2 toScreenSpace(ivec2 in) const {
		in += getPosContent();
		if (this->parent != NULL) {
			in = this->parent->toScreenSpace(in);
		}
		return in;
	}
	guibase* getByID(int id) {
		auto it = std::find_if(guis.begin(), guis.end(), [id](auto g) {
			return g->id == id;
		});
		if (it == guis.end()) {
			return nullptr;
		}
		return *it;
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
	template<typename Container>
	void sortChildrenByList(Container& container) {
		//TODO: very inefficient
		std::sort(guis.begin(), guis.end(), [&container](guibase* a, guibase* b) {
			int indexA = indexOfCtr(container, a);
			int indexB = indexOfCtr(container, b);
			if (indexA < 0)
				return true;
			if (indexB < 0)
				return false;
			return indexA < indexB;
		});
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
				if (!gui->isVisible())
					continue;
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
			if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
				evt.requestFocus(this);
				return true;
			}
			if (canMouseHit()) {
				evt.requestFocus(this);
				return true;
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
	void addProperties(Table::tbl* table) override;
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

