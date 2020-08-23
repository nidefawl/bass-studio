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
class g_layout_ctr_drop_area : public i_ctr_drop_area {
public:
	g_layout_ctr_drop_area(i_ctr_layout* _parent)
	  : i_ctr_drop_area(_parent) {
	}
	~g_layout_ctr_drop_area() {

	}
};
class guictr_layout : public guictr_base, public i_ctr_layout {
	bool setOverlayPos(g_layout_ctr_drop_area* area, const dock_pos dockPos);
	bool setOverlayPosForTab(g_layout_ctr_drop_area* area, const dock_pos dockPos, const int32_t dockOffset, const bool rightSideHandle);
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
//		setBackgroundRenderedInset(true);
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
		std::vector<std::shared_ptr<guictr_layout_entry>> entriesToRemove;
		for (auto entry : entries) {
			auto guiCtrLayout = dynamic_cast<guictr_layout*>(entry->getGui());
			if (guiCtrLayout) {
				guiCtrLayout->simplify();
				if (guiCtrLayout->getEntries().empty()) {
					entriesToRemove.push_back(entry);
				}
			}
		}
		if (entriesToRemove.size()) {
			log_printf("remove %d container entries\n", entriesToRemove.size());
		}
		for (auto entry : entriesToRemove) {
			std::shared_ptr<guictr_layout_entry> out;
			getContainerRef(entry.get(), out, true);
		}
		if (entries.size() < 2)
			setLayout(container_layout::SOLE);
//		if (this->ctrLayout != container_layout::TABBED) {
//			for (auto handle : handles) {
//				handle->setVisible(false);
//			}
//		}
	}
	void setLayout(container_layout ctrLayoutNew) {
		this->ctrLayout = ctrLayoutNew;
		updateVisible();
	}
	void postContentChanged() {
		simplify();
		updateVisible();
		layout();
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

	void onChildLayoutChanged(guibase* g) override {
		postContentChanged();
		if (this->parent) {
			this->parent->onChildLayoutChanged(g);
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

	virtual bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    container_layout getLayout() const override {
    	return this->ctrLayout;
    }
	bool getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) override;
	void addEntry(std::shared_ptr<guictr_layout_entry> ctr, int32_t posOffset = -2);
	bool placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area) override;

	void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& handles) override;
//	void replaceContentWith(guictr_layout* ctr);

	std::shared_ptr<guictr_layout_entry> replaceContainerWith(guictr_base* ctr, std::shared_ptr<guictr_layout> newContainer);
};
struct my_test_ctr;
class guictr_layout_entry_handle : public guibutton {
	my_test_ctr* const parentCtr;
	guictr_base* const ctr;
	bool hasDragged = false;
	bool hasClicked = false;
public:
	guictr_layout_entry_handle(my_test_ctr* _parentCtr, guictr_base* _ctr) : parentCtr(_parentCtr), ctr(_ctr) {
		setText(_ctr->getLabel());
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
	std::shared_ptr<guictr_base> ctr; /* non-owning */ //TODO: make this owning, unique ptr
	guictr_layout_entry_handle* ctrHandle;
	my_test_ctr(std::shared_ptr<guictr_base> _ctr)
	    : ctr(_ctr) {
		ctrHandle = new guictr_layout_entry_handle(this, _ctr.get());
//		if (dynamic_cast<guictr_layout*>(_ctr.get()) == nullptr) {
//			ctrHandle->setLabel("...");
//		} else {
//			ctrHandle = nullptr;
//		}
	}
	~my_test_ctr() {
		delete ctrHandle;
	}
	guictr_base* getGui() override {
		return ctr.get();
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
