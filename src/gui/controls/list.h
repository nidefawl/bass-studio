#pragma once
#include <algorithm>
#include <utility>
#include <nanovg.h>
#include "gui/gui.h"
#include "gui/container/container.h"
#include "renderresources.h"
#include "scrollbar.h"
#include "seq_util.h"


class gui_list_entry : public guibase {
    friend class gui_list;

protected:
    int32_t icon       = -1;
    bool selected      = false;
    int32_t entryDepth = 0;

public:
    gui_list_entry() : guibase() {
        setCanMouseHit(true);
    }
    ~gui_list_entry() override = default;
    void render(NVGcontext* vg) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void dragMoveOn(guibase* target, ivec2 mousepos)    override = 0;
    void dragReleaseOn(guibase* target, ivec2 mousepos) override = 0;
    virtual String getText()                                    = 0;
    void setDepth(int32_t depth) {
        entryDepth = depth;
    }
};

class gui_list_folder_entry : public gui_list_entry {
    const String string;
    bool bIsOpened = false;
public:
    explicit gui_list_folder_entry(String str) : gui_list_entry(), string(std::move(str)) {
        setGuiType(gui_type::GUI_TYPE_LIST_FOLDER);
        label = string;
        setBackgroundRendered(true);
        icon = ICON_FOLDER;
    }
    void dragMoveOn(guibase* target, ivec2 mousepos) override {
    }
    void dragReleaseOn(guibase* target, ivec2 mousepos) override {
    }
    String getText() override {
        return string;
    }
    bool isOpened() const {
        return bIsOpened;
    }
    void setIsOpened(const bool opened) {
        bIsOpened = opened;
        icon = opened ? ICON_FOLDER_OPEN : ICON_FOLDER;
    }
};

class gui_list : public guictr_base, public gui_scrollcontainer {
protected:
    gui_scrollbar scrollbar;
    std::vector<gui_list_entry*> listGuis;
    int32_t first       = 0;
    int32_t last        = 0;
    int rowHeight       = 30;
    ivec4 rowMargin     = { 0, 0, 0, 0 };
    bool renderHR       = false;
    int32_t selectedIdx = -1;
    bool bOwnsListEntries = true;
public:
    gui_list() : guictr_base(), scrollbar(1, 0.0f, *this) {
        add(&scrollbar);
        setBackgroundRendered(true);
    }
    ~gui_list() override {
        remove(&scrollbar);
        if (bOwnsListEntries)
            destroyGuis();
        else 
            removeGuis();
    }
    void setOwnsListEntries(bool _bOwnsListEntries) {
        bOwnsListEntries = _bOwnsListEntries;
    }
    template<typename Comparator>
    void sort(Comparator comparator) {
        std::stable_sort(listGuis.begin(), listGuis.end(), comparator);
        guictr_base::sortChildrenByList(listGuis);
    }
    int32_t getSelectedIdx() {
        return selectedIdx;
    }
    void setSelectedIdx(int32_t selectedIdx) {
        this->selectedIdx = selectedIdx;
    }
    void setRowMargin(ivec4 _rowMargin) {
        rowMargin = _rowMargin;
    }
    void setRenderHR(bool _renderHR) {
        renderHR = _renderHR;
    }
    void setRowHeight(int h) {
        rowHeight = h;
    }
    int getRowHeight() const {
        return rowHeight;
    }
    ivec2 getScrollTotalSize() const override {
        ivec2 cs = getSizeContent();
        cs.y     = rowHeight * (int32_t) listGuis.size();
        return cs;
    }
    ivec2 getScrollViewSize() const override {
        return getSizeContent();
    }
    void updateVisible();

    void scrollOffsetChanged(int dir, float offset) override;

    void render(NVGcontext* vg) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void setList(const std::vector<gui_list_entry*>& _newList) {
        //newList may contain pointers that are already added

        //make a copy of current loaded guis
        std::vector<gui_list_entry*> listGuisDelete = listGuis;

        //remove all existing from that copy
        removeAll(listGuisDelete, _newList);

        removeAll(listGuis, listGuisDelete);
        //delete entries that are gone
        for (gui_list_entry* g : listGuisDelete) {
            remove(g);
            if (bOwnsListEntries)
                delete g;
        }

        // remove all entries
        for (gui_list_entry* g : listGuis) {
            remove(g);
        }

        // add all entries
        listGuis = _newList;
        for (gui_list_entry* g : _newList) {
            add(g);
        }

        // fix selected index
        if (selectedIdx >= (int32_t) listGuis.size()) {
            selectedIdx = CtrSize(listGuis) - 1;
        }

        layout();
    }
    void layout() override {
        ivec2 cs       = getSizeContent();
        int scrollW    = math::max(5, math::min(cs.x / 10, gui_scrollbar::defaultW));
        int entryW     = cs.x - scrollW;
        scrollbar.size = ivec2(scrollW, cs.y);
        scrollbar.pos  = ivec2(cs.x - scrollW, 0);

        int x = 0;
        int y = 0;
        for (guibase* gui : guis) {
            if (gui == &scrollbar)
                continue;
            gui->pos  = ivec2(x + rowMargin.x, y + rowMargin.y);
            gui->size = ivec2(entryW - (rowMargin.x + rowMargin.z), rowHeight - (rowMargin.y + rowMargin.w));

            y += rowHeight;
        }
        for (guibase* gui : guis) {
            gui->layout();
        }
        updateVisible();
    }
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
    void buttonClicked(guibase* button) override;

    guibase* getFocusedContainer() override;

    std::vector<gui_list_entry*>& getListRef() {
        return listGuis;
    }

    gui_list_entry* getSelectedEntry() {
        if (selectedIdx < 0 || selectedIdx >= (int32_t) listGuis.size())
            return nullptr;
        return listGuis[selectedIdx];
    }
};
