#pragma once
#include <initializer_list>
#include <nanovg_min.h>
#include <vector>
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


class guictr_stacked : public guictr_base, public splitter_cb {
    struct stacked_entry;
    std::vector<stacked_entry*> entries;
    bool bVerticalLayout = true;
    float titleHeight = 0.0f;
public:
    static constexpr int32_t STACK_ENTRY_BTN_SIZE = 24;
    guictr_stacked() : guictr_base() {
        padding = 0;
        margin = 0;
        setVerticalLayout(false);
        setBackgroundRendered(false);
        setFlag(FLG_RENDER_LABEL, true);
    }
    ~guictr_stacked();
    void removeGuis() override;
    void setVerticalLayout(bool bVertical) {
        this->bVerticalLayout = bVertical;
    }
    int32_t getNumEntries();
    void toggleEntry(int32_t idx);
    void addEntry(guibase* ctr);
    void removeEntries();
    void buttonClicked(guibase* button) override;
    void layout() override;
    void render(NVGcontext* vg) override;
    void renderBackground(NVGcontext* vg) override;
    void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override;
    ivec2 getContainerPos() override;
    ivec2 getContainerSize() override;
    int32_t getCollapsedCtrHeight(guictr_base* ctr);
    void updateSplitterPositions();
    void setSplitters(const std::vector<float>& splitterPos);
    void getSplitterPositions(std::vector<float>& splitterPos);
    void renderContainerLabel(NVGcontext* vg) override {
        const auto h = titleHeight;
        if (isFlag(FLG_RENDER_LABEL) && label.length() && h > 0) {
            const auto bgColor = getInnerBackgroundColorFromState(getStateFlags());
            renderTextLabel(vg,
                            vec2(getPosContent()) + vec2(padding + 2, h / 2.0),
                            vec2(getSizeContent()) - vec2(INSET_TITLE + 2, 0),
                            label,
                            theme,
                            h,
                            theme->getContrastColor(bgColor),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }
    void setTitleHeight(float height) {
        titleHeight = height;
    }
    virtual float getTitleHeight() const {
        return !bVerticalLayout ? 0.0f : (label.empty() ? 0.0f : titleHeight);
    }
};
class guictr_tabbed : public guictr_base {

    struct tabbed_entry;
    std::vector<tabbed_entry*> entries;
    tabbed_entry* activeEntry = nullptr;
    ivec2 sizeContentTab{ 0 };
    ivec2 insetMenuBar{ 0 };
    bool hasDragged = false;

public:
    guictr_tabbed() : guictr_base() {
        setCanMouseHit(true);
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
