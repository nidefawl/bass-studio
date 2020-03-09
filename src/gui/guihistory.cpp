#include <glm/geometric.hpp>
#include <nanovg.h>
#include <vector>
#include <memory>
#include <numeric>

#include "str_util.h"
#include "saferef.h"
#include "seq_util.h"
#include "color_util.h"
#include "event.h"
#include "mouse.h"
#include "gui.h"
#include "button.h"
#include "textfield.h"
#include "guicolors.h"
#include "guicontainer.h"
#include "guiconstant.h"
#include "list.h"
#include "guicolorpick.h"
#include "guiscrollcontainer.h"
#include "logging.h"
#include "edithistory.h"
#include "host/mainctrl.h"

class gui_list_history : public gui_list {
public:
	gui_list_history() : gui_list()  {
	}
	~gui_list_history() {

	}
	void buttonClicked(guibase* button) {
		selectedIdx = indexOfCtr(this->listGuis, button);
		if (selectedIdx > -1) {
//			if (parent) parent->buttonClicked(button);
		}
	}
};
class gui_history_list_entry_t : public gui_list_entry {
public:
	action_base* const entry;
	gui_history_list_entry_t(action_base* const _entry) : gui_list_entry(), entry(_entry) {
		icon = 0;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
//		target->pluginEntryDragMove(this, mousepos);
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
//		target->pluginEntryDragRelease(this, mousepos);
	}
	String getText() override {
		return entry->getDesc();
	}
};
class guictr_history_view : public guictr_base {

	gui_list_history historyList;
	guictr_scrollbar scrollContainer;
	int64_t histRevision = -1;
public:
	guictr_history_view() : guictr_base(), scrollContainer() {
		padding = 0;
		margin = 0;
		add(&scrollContainer);
		scrollContainer.add(&historyList);
		scrollContainer.maxHeight = -1;
	}
	~guictr_history_view() {
		removeGuis();
	}
	void rebuildList() {
		std::vector<gui_list_entry*> _newList;
		DawInstance *daw = DawInstance::get();
		int32_t selectedIdx = -1;
		if (daw) {
			auto& editHistory = daw->getHist();
			std::vector<action_base*> m_undo;
			std::vector<action_base*> m_redo;
			editHistory.getActions(m_undo, m_redo);
			for (auto it = m_redo.begin(); it != m_redo.end(); it++) {
				action_base* entry = *it;
				gui_history_list_entry_t* g = new gui_history_list_entry_t(entry);
				_newList.push_back(g);
			}
			for (auto it = m_undo.rbegin(); it != m_undo.rend(); it++) {
				action_base* entry = *it;
				gui_history_list_entry_t* g = new gui_history_list_entry_t(entry);
				_newList.push_back(g);
			}
			selectedIdx = m_redo.size();
			histRevision = editHistory.getRevision();
		}
		historyList.setList(_newList);
		historyList.setSelectedIdx(selectedIdx);
		layout();
	}
	void onTick(AppCtrl* ctrl) override {
		DawInstance *daw = DawInstance::get();
		if (daw) {
			if (histRevision != daw->getHist().getRevision()) {
				rebuildList();
			}
		}
	}
	void render(NVGcontext* vg) {
		if (!setScissorTransform(vg)) {
			return;
		}
		for (auto c : guis) {
			nvgSave(vg);
			c->render(vg);
			nvgRestore(vg);
		}
		//int colorIdx = 0;
		//auto renderDebugF = [](NVGcontext* vg, guibase* gui, NVGcolor color) {
		//	nvgBeginPath(vg);
		//	nvgRect(vg, gui->pos.x, gui->pos.y, gui->size.x, gui->size.y);
		//	nvgFillColor(vg, color);
		//	nvgFill(vg);
		//};
		//static NVGcolor dbgcolorsa[5] = {
		//	nvgRGBA(255, 0, 0, 55),
		//	nvgRGBA(0, 255, 0, 55),
		//	nvgRGBA(0, 0, 255, 55),
		//	nvgRGBA(255, 0, 255, 55),
		//	nvgRGBA(255, 255, 0, 55)
		//};

		//for (guibase* g : guis) {
		//	//renderDebugF(vg, g, dbgcolorsa[colorIdx++ % 5]);
		//}
	}
	virtual void buttonClicked(guibase* button) {
	}
	void layout() override {
		ivec2 size = getSizeContent();
		int32_t hTop = HEIGHT_DEFAULT_INPUT;
		scrollContainer.pos = {0, hTop};
		scrollContainer.size = {size.x, size.y-hTop};
		scrollContainer.determineSize(scrollContainer.size);

		for (auto c : guis) {
			c->layout();
		}
	}
	virtual void setControl(BaseCtrl* parentCtrl) {
		guictr_base::setControl(parentCtrl);

	}
};
guictr_base* makeCtrHistory() {
	guictr_history_view* ctr = new guictr_history_view();
	return ctr;
}
