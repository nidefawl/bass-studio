#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>

#include "math/vec.h"
#include "math/seq_math.h"

#include "str_util.h"
#include "color_util.h"

#include "mouse.h"
#include "event.h"
#include "exceptions.h"
#include "renderresources.h"

#include "basectrl.h"

#include "guiglobals.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/knob.h"

#include "gui/guicontainer.h"
#include "gui/guicontainer_layout.h"
#include "gui/guicolorpick.h"
#include "gui/guiinputfield.h"
#include "gui/button.h"
#include "gui/splitter.h"
#if BUILD_VSTHOST
#include "gui/debugctr.h"
#endif
#include "gui/splitter.h"


class guictr_layout;
class guictr_layout_dock_overlay_impl : public guictr_layout_dock_overlay {
	public:
		class gui_dock_overlay_control : public guibutton {
			const dock_pos dockpos;
		public:
			gui_dock_overlay_control(dock_pos _dockpos) : dockpos(_dockpos) {

			}
			dock_pos getDockPos() const {
				return dockpos;
			}
			void render(NVGcontext* vg) {
				guibutton::render(vg);
			}

		};
	public:
		std::vector<gui_dock_overlay_control*> controls;
	private:
		void addControl(gui_dock_overlay_control* ctrl) {
			add(ctrl);
			controls.push_back(ctrl);
		}
	public:
		guictr_layout* parentguictrlayout=nullptr;
		guictr_layout_dock_overlay_impl() {
			guis.reserve(5);
			controls.reserve(5);
			addControl(new gui_dock_overlay_control(dock_pos::CENTER));
			addControl(new gui_dock_overlay_control(dock_pos::LEFT));
			addControl(new gui_dock_overlay_control(dock_pos::RIGHT));
			addControl(new gui_dock_overlay_control(dock_pos::TOP));
			addControl(new gui_dock_overlay_control(dock_pos::BOTTOM));
		}
		~guictr_layout_dock_overlay_impl() {
			destroyGuis();
		}
		void setPositions(std::vector<dock_pos> positions) {
			for (auto* dockCtrl : controls) {
				dockCtrl->setVisible(STL_CONTAINS(positions, dockCtrl->getDockPos()));
			}
		}
		void layout() {
			ivec2 cs = getSizeContent();
			ivec2 wh(32);
			ivec2 spacing(4);
			for (auto* dockCtrl : controls) {
				dockCtrl->size = wh;
				switch (dockCtrl->getDockPos()) {
				case dock_pos::CENTER:
					dockCtrl->pos = ivec2(cs/2-wh/2);
					break;
				case dock_pos::LEFT:
					dockCtrl->pos = ivec2(cs/2-wh/2-ivec2(wh.x+spacing.x, 0));
					break;
				case dock_pos::RIGHT:
					dockCtrl->pos = ivec2(cs/2-wh/2+ivec2(wh.x+spacing.x, 0));
					break;
				case dock_pos::TOP:
					dockCtrl->pos = ivec2(cs/2-wh/2-ivec2(0, wh.y+spacing.y));
					break;
				case dock_pos::BOTTOM:
					dockCtrl->pos = ivec2(cs/2-wh/2+ivec2(0, wh.y+spacing.y));
					break;
				default:
					dbgassert(0 && "Unhandled switch case");
					break;
				}
			}
			for (guibase* gui : guis) {
				gui->layout();
			}
		}
};
class g_layout_ctr_drop_area : public i_ctr_drop_area {
public:
	g_layout_ctr_drop_area(i_ctr_layout* _parent)
	  : i_ctr_drop_area(_parent) {
	}
	~g_layout_ctr_drop_area() {

	}
};
class guictr_layout : public guictr_base, public i_ctr_layout {
	bool makeOverlay(g_layout_ctr_drop_area* area, const dock_pos dockPos, const int32_t dockOffset);
	g_layout_ctr_drop_area* makeDropArea(int32_t idx);
public:
private:
	container_layout ctrLayout = container_layout::SOLE;
	int32_t activePosition = -1;
	std::vector<std::shared_ptr<guictr_layout_entry>> entries;
	std::vector<guibase*> handles;
	std::vector<std::shared_ptr<g_layout_ctr_drop_area>> dragdropContainerAreaHelpers;
public:
	guictr_layout() : guictr_base() {
		setBackgroundRendered(false);
		setBackgroundRenderedInset(true);
		this->setCanMouseHit(true);
//		dragdropContainerAreaHelpers.resize(16);
		margin = 0;
		padding = 0;
	}
	virtual ~guictr_layout() {
		removeGuis();
		entries.clear();
	}
	std::vector<std::shared_ptr<guictr_layout_entry>>& getEntries() {
		return entries;
	}
	void simplify() {
		if (entries.size() < 2)
			setLayout(container_layout::SOLE);
	}
	void setLayout(container_layout ctrLayoutNew) {
		this->ctrLayout = ctrLayoutNew;
		updateVisible();
	}
	void setActiveEntry(int32_t idx) {
		this->activePosition = idx;
		updateVisible();
		if (this->parent) {
			layout();
		}
	}
	void updateVisible() {
		int32_t entryIdx = 0;
		for (auto &entry : entries) {
			entry->getGui()->setVisible(this->ctrLayout != container_layout::TABBED || entryIdx == this->activePosition);
			entryIdx++;
		}
	}
	void layout() override;
	void buttonClicked(guibase* button) override
	{
		if (this->ctrLayout == container_layout::TABBED) {
			int32_t pos = 0;
			for (auto &entry : entries) {
				if (entry->getHandle() == button) {
					setActiveEntry(pos);
					return;
				}
				pos++;
			}
		}
	}

	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : handles) {
				if (!gui->isVisible())
					continue;
				if (gui->mouseHitTest(localMouse, evt)) {
					return true;
				}
			}
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
    container_layout getLayout() const override {
    	return this->ctrLayout;
    }
    bool getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) override
    {
		auto it = std::find_if(this->entries.begin(), this->entries.end(), [ctr](auto& e){
			return ctr == e.get();
		});
		if (it == entries.end()) {
			//throw applogicexception(StringFormat("%s - attempt to remove non-present element", StringAsCStr(getClassName())));
            dbgassert(0);
            return false;
		}
        out = *it;
        if (!remove) {
            return true;
        }
		entries.erase(it);
		guictr_base::remove(ctr->getGui());
		auto* guiHandle = ctr->getHandle();
		if (guiHandle) {
			removeEntry(handles, guiHandle);
			guictr_base::remove(guiHandle);
		}
		ctr->parentLayoutContainer = nullptr;
        simplify();
		layout();
		return true;
	}
	void addEntry(std::shared_ptr<guictr_layout_entry> ctr, int32_t posOffset = -2) {

		auto it = std::find(entries.begin(), entries.end(), ctr);
		if (it != entries.end()) {
			throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
		}
		auto insertPos = posOffset == -1 ? entries.begin() : (posOffset == -2 || posOffset >= entries.size() ? entries.end() : (entries.begin()+posOffset));
		entries.insert(insertPos, ctr);
		guictr_base::add(ctr->getGui());
		auto* guiHandle = ctr->getHandle();
		if (guiHandle) {
			guictr_base::add(guiHandle);
			handles.push_back(guiHandle);
		}
		ctr->parentLayoutContainer = this;
	}
	bool placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area) override {
		auto itHelper = std::find_if(dragdropContainerAreaHelpers.begin(), dragdropContainerAreaHelpers.end(), [area](std::shared_ptr<g_layout_ctr_drop_area>& e) {
			return e.get() == area;
		});

		if (itHelper == dragdropContainerAreaHelpers.end()) {
			throw applogicexception("pointer area does not point to a member of this container");
		}
		// prevent dropping into self
		guibase* parent = this;
		while (parent) {
			if (parent == ctr.get()->getGui()) {
				return false;
			}
			parent = parent->parent;
		}
		std::shared_ptr<g_layout_ctr_drop_area>& dropArea = *itHelper;
		dock_pos dockPos = dropArea->dockPos;
		int32_t dockPosOffset = dropArea->dockPosOffset;
		auto updatedCtrLayout = dock_pos_to_container_layout(dockPos);
		if (entries.size() > 1 && updatedCtrLayout != ctrLayout) {
			// introduce new layer
			log_printf("Introduce new layer\n", 0);
//			std::make_shared<guictr_layout> ctrLayoutNewLayer;
//			this->getContainerRef(ctr, out, remove)
			switch (dockPos) {
			case dock_pos::LEFT:
				// set container layout to SPLIT_V, make this container right entry, entry to drop left
				break;
			case dock_pos::RIGHT:
				break;
			case dock_pos::TOP:
				break;
			case dock_pos::BOTTOM:
				break;
			}
			return false;
		}
		auto it = std::find(entries.begin(), entries.end(), ctr);
		if (it != entries.end()) {
			throw applogicexception(StringFormat("%s - attempt to add element twice", StringAsCStr(getClassName())));
		}
		std::shared_ptr<guictr_layout_entry> out;
		if (!ctr->getContainerRef(out, true)) {
			return false;
		}
//		hasRemovedContainer = true;
//		dbgassert(ctrContent2);
//		dbgassert(ctrContent == ctrContent2);
		bool updatedVisible = false;
		if (ctrLayout != updatedCtrLayout) {
			log_printf("Container layout changed to %d\n", static_cast<int>(updatedCtrLayout));
			setLayout(updatedCtrLayout);
			updatedVisible = true;
		}
		if (dockPos == dock_pos::STACK) {
			if (dockPosOffset <= -1) {
				entries.insert(entries.begin(), ctr);
			} else if (dockPosOffset >= entries.size()) {
				entries.insert(entries.end(), ctr);
			} else {
				entries.insert(entries.begin() + dockPosOffset, ctr);
			}
		} else if (dockPos == dock_pos::RIGHT || dockPos == dock_pos::BOTTOM) {
			entries.insert(entries.end(), ctr);
		} else {
			entries.insert(entries.begin(), ctr);
		}
//		ctr->getGui()->setFlag(FLG_RENDER_LABEL, true);
		guictr_base::add(ctr->getGui());
		auto* guiHandle = ctr->getHandle();
		if (guiHandle) {
			guictr_base::add(guiHandle);
			handles.push_back(guiHandle);
		}
		ctr->parentLayoutContainer = this;
		if (!updatedVisible) {
			updateVisible();
		}
		layout();

		return true;
	}

	void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& handles) override;

};
struct my_test_ctr;
class guictr_layout_entry_handle : public guibutton {
	my_test_ctr* const parentCtr;
	guictr_base* const ctr;
	bool hasDragged = false;
	bool hasClicked = false;
public:
	guictr_layout_entry_handle(my_test_ctr* _parentCtr, guictr_base* _ctr) : parentCtr(_parentCtr), ctr(_ctr) {
		setText("...");
//		setEnabledRef(&active);
////		setFontScale(0.7f);
	}
	~guictr_layout_entry_handle() = default;
	void handleDraggedBegin(MouseEvent &evt) override;
	void handleDraggedMove(MouseEvent &evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
	int32_t getStateFlags() const override {
		int32_t state = guibuttonbase::getStateFlags();
//		if (active()) {
//			state |= FLG_ACT;
//		}
		return state;
	}
	void render(NVGcontext* vg) override;
};
struct my_test_ctr : public guictr_layout_entry {
	guictr_base* const ctr; /* non-owning */
	guictr_layout_entry_handle* const ctrHandle;
	my_test_ctr(guictr_base* _ctr) : ctr(_ctr), ctrHandle(new guictr_layout_entry_handle(this, _ctr)) {

		ctrHandle->setLabel("...");
	}
	~my_test_ctr() {
		delete ctrHandle;
	}
	guibase* getGui() override {
		return ctr;
	}
	guibase* getHandle() override {
		return ctrHandle;
	}
	std::shared_ptr<guictr_layout_entry> duplicateContainer() override {
		return nullptr;
	}
	bool getContainerRef(std::shared_ptr<guictr_layout_entry>& out, bool remove) override {
        return parentLayoutContainer->getContainerRef(this, out, remove);
	}
};
class guibutton_drag: public guibutton {
public:
	guibutton_drag() :
			guibutton() {

	}
};
