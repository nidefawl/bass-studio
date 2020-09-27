#pragma once
#include "gui/button.h"
#include "gui/guicontainer.h"

struct guictr_layout_entry;

using ContainerBuilder = std::function<std::shared_ptr<guictr_base> ()>;
std::map<container_type, ContainerBuilder>& getContainerFactory();
std::shared_ptr<guictr_layout_entry> createGuiCtrLayoutEntry(std::shared_ptr<guictr_base> ctr);
class guictr_layout_entry_handle : public guibutton {
	guictr_layout_entry* const parentCtr;
	guictr_base* const ctr;
	bool hasDragged = false;
	bool hasClicked = false;
public:
	guictr_layout_entry_handle(guictr_layout_entry* _parentCtr, guictr_base* _ctr) : parentCtr(_parentCtr), ctr(_ctr) {
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
