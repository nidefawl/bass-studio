#include "host/mainctrl.h"
#include "edithistory.h"
#include "gui/container/container.h"
#include "gui/controls/list.h"
#include "gui/container/scrollcontainer.h"

class gui_list_history : public gui_list {
public:
    gui_list_history() : gui_list() {
    }
    ~gui_list_history() override = default;
    void buttonClicked(guibase* button) override {
        selectedIdx = indexOfCtr(this->listGuis, button);
        if (selectedIdx > -1) {
            //if (parent) parent->buttonClicked(button);
        }
    }
};
class gui_history_list_entry_t : public gui_list_entry {
public:
    String desc;
    gui_history_list_entry_t(action_base* const _entry)
        : gui_list_entry(), desc(_entry->getDesc()) {
        icon = 0;
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
//        target->pluginEntryDragMove(this, toControlsObjectSpace(mousepos, target));
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
//        target->pluginEntryDragRelease(this, toControlsObjectSpace(mousepos, target));
    }
    String getText() override {
        return desc;
    }
};
class guictr_history_view : public guictr_base {

    gui_list_history historyList;
    guictr_scrollbar scrollContainer;
    int64_t histRevision = -1;

public:
    guictr_history_view() : guictr_base(), scrollContainer() {
        ctrType = CTR_TYPE_HISTORY;
        padding = 0;
        margin  = 0;
        add(&scrollContainer);
        scrollContainer.add(&historyList);
        scrollContainer.maxHeight = -1;
    }
    ~guictr_history_view() override {
        removeGuis();
    }
    void rebuildList() {
        std::vector<gui_list_entry*> _newList;
        auto const daw = dawCtrl->getDaw();
        int32_t selectedIdx = -1;
        auto& editHistory = daw->getHist();
        std::vector<action_base*> m_undo;
        std::vector<action_base*> m_redo;
        editHistory.getActions(m_undo, m_redo);
        for (auto it = m_redo.begin(); it != m_redo.end(); ++it) {
            _newList.push_back(new gui_history_list_entry_t(*it));
        }
        for (auto it = m_undo.rbegin(); it != m_undo.rend(); ++it) {
            _newList.push_back(new gui_history_list_entry_t(*it));
        }
        selectedIdx  = m_redo.size();
        histRevision = editHistory.getRevision();
        historyList.setList(_newList);
        historyList.setSelectedIdx(selectedIdx);
        layout();
    }
    void onTick(AppCtrl* ctrl) override {
        if (histRevision != dawCtrl->getDaw()->getHist().getRevision()) {
            rebuildList();
        }
    }
    void render(NVGcontext* vg) override {
        if (histRevision != dawCtrl->getDaw()->getHist().getRevision()) {
            return;
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto c : guis) {
            nvgSave(vg);
            c->render(vg);
            nvgRestore(vg);
        }
    }
    void buttonClicked(guibase* button) override {
    }
    void layout() override {
        ivec2 size           = getSizeContent();
        int32_t hTop         = HEIGHT_DEFAULT_INPUT;
        scrollContainer.pos  = { 0, hTop };
        scrollContainer.size = { size.x, size.y - hTop };
        scrollContainer.determineSize(scrollContainer.size);

        for (auto c : guis) {
            c->layout();
        }
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
    }
};
guictr_base* makeCtrHistory() {
    guictr_history_view* ctr = new guictr_history_view();
    return ctr;
}
