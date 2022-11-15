#pragma once
#include <nanovg_min.h>
#include "math/vec.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "guiconstant.h"
#include "gui/gui.h"
#include "gui/controls/splitter.h"

class BaseCtrl;
struct guitheme_t;


class guictr_stacked final : public guictr_base, public splitter_cb {
    struct stacked_entry;
    std::vector<stacked_entry*> entries;

public:
    static constexpr int32_t STACK_ENTRY_BTN_SIZE = 24;
    guictr_stacked() : guictr_base() {
    }
    ~guictr_stacked();

    int32_t getNumEntries();
    void toggleEntry(int32_t idx, int flags);
    void addEntry(guictr_base* ctr, String title);
    void buttonClicked(guibase* button) override;
    void layout() override;
    void render(NVGcontext* vg) override;
    void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override;
    ivec2 getContainerPos() override;
    ivec2 getContainerSize() override;
    int32_t getCollapsedCtrHeight(guictr_base* ctr);
};
class guictr_tabbed final : public guictr_base {

    struct tabbed_entry;
    std::vector<tabbed_entry*> entries;
    tabbed_entry* activeEntry = nullptr;
    ivec2 sizeContentTab{ 0 };
    ivec2 insetMenuBar{ 0 };
    bool hasDragged = false;

public:
    guictr_tabbed() : guictr_base() {
        setCanMouseHit(true);
        setDragRendered(true);
    }
    ~guictr_tabbed();
    void setTabMenuInset(ivec2 offset) {
        this->insetMenuBar = offset;
    }
    int32_t getNumEntries();
    void setActiveEntry(int32_t idx);
    void addEntry(guibase* ctr, String title);
    void buttonClicked(guibase* button) override;
    void layout() override;
    void render(NVGcontext* vg) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    //void handleDraggedRelease(MouseEvent& evt);
};
