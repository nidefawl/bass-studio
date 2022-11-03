#pragma once
#include <vector>
#include <memory>
#include <algorithm>

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
#include "gui/controls/knob.h"

#include "container.h"
#include "gui/controls/splitter.h"
#include "gui/container/container_layout_types.h"
#include "gui/container/container_layout_snapshot.h"
#include "tls.h"


class guictr_layout;
struct dawview_layout_t;

class guictr_layout : public guictr_base, public i_ctr_layout, public splitter_cb {
    bool setOverlayPos(i_ctr_drop_area* area, dock_pos dockPos, ivec2 overlayPos, ivec2 overlaySize, int32_t dockPosOfffset, int32_t childContainerIndex);
    bool setOverlayPosForTab(i_ctr_drop_area* area, dock_pos dockPos, int32_t dockOffset, bool rightSideHandle);
    i_ctr_drop_area* makeDropArea(int32_t idx);

private:
    container_layout ctrLayout = container_layout::SOLE;
    int32_t activePosition     = -1;
    std::vector<std::shared_ptr<guictr_layout_entry>> entries;
    std::vector<guibase*> handles;
    std::vector<std::shared_ptr<i_ctr_drop_area>> dragdropContainerAreaHelpers;
    std::vector<std::shared_ptr<Splitter>> splitters;
    String getLayoutCtrName() {
        if (this->label.empty()) {
            auto ctrName = getClassName();
            return ctrName;//StringFormat("%12zX", reinterpret_cast<uint64_t>(this));
        }
        return this->label;
    }
    Splitter* getSplitter(int32_t pos);
    bool bHideHandlesWhenLocked = false;
public:
    guictr_layout();
    ~guictr_layout() override {
        removeGuis();
        entries.clear();
    }
    bool isHandleShown() const;
    void setHideHandlesWhenLocked(bool b) { bHideHandlesWhenLocked = b; }
    ivec2 paddingTL(int _padding) const override {
        if (parentCtrl && parentCtrl->isDraggingContainer()) {
            return ivec2(8);
        }
        //return ivec2(0, _padding);
        //return ivec2(_padding - margin*snapSides.x, _padding - margin*snapSides.y);
        return {_padding - margin * snapSides.x, 0};
    }
    ivec2 paddingBR(int _padding) const override {
        if (parentCtrl && parentCtrl->isDraggingContainer()) {
            return ivec2(8);
        }
        //return ivec2(0, _padding);
        //return ivec2(_padding - margin*snapSides.z, _padding - margin*snapSides.w);
        return {_padding - margin * snapSides.z, 0};
    }
    void removeAllEntries() {
        for (auto& entry: entries) {
            guictr_base::remove(entry->getGui());
            auto* guiHandle = entry->getHandle();
            if (guiHandle) {
                removeEntry(handles, guiHandle);
                guictr_base::remove(guiHandle);
            }
            entry->parentLayoutContainer = nullptr;
        }
        entries.clear();
        handles.clear();
        activePosition = -1;
    }
    int32_t getActivePosition() const {
        return activePosition;
    }
    std::vector<std::shared_ptr<guictr_layout_entry>>& getEntries() {
        return entries;
    }
    std::vector<float> getSplitterPositions();
    void setSplitterPositions(std::vector<float>& splitterPositons);
    void simplify() {
        struct InlineEntry {
            int32_t index = 0;
            std::shared_ptr<guictr_layout_entry> entry;
        };
        std::vector<std::shared_ptr<guictr_layout_entry>> entriesToRemove;
        std::vector<InlineEntry> entriesInsert;
        int32_t index = 0;
        for (const auto& entry: entries) {
            auto guiCtrLayout = dynamic_cast<guictr_layout*>(entry->getGui());
            if (guiCtrLayout) {
                guiCtrLayout->simplify();
                auto& childEntries = guiCtrLayout->getEntries();
                if (childEntries.empty()) {
                    entriesToRemove.push_back(entry);
                } else if (childEntries.size() == 1) {
                    auto& childEntry = childEntries[0];
                    InlineEntry inlineEntry;
                    guiCtrLayout->getContainerRef(childEntry.get(), inlineEntry.entry, true);
                    inlineEntry.index = index;
                    entriesToRemove.push_back(entry);
                    entriesInsert.push_back(inlineEntry);
                }
            }
            index++;
        }
        if (!entriesToRemove.empty()) {
            log_lf(Log::L_DEBUG, "remove %zu container entries\n", entriesToRemove.size());
        }
        for (const auto& entry: entriesToRemove) {
            std::shared_ptr<guictr_layout_entry> out;
            getContainerRef(entry.get(), out, true);
        }
        for (const auto& entry: entriesInsert) {
            addEntry(entry.entry, entry.index);
        }
        if (entries.size() < 2) {
            setLayout(container_layout::SOLE);
        }
        //if (this->ctrLayout != container_layout::TABBED) {
        //for (auto handle : handles) {
        //handle->setVisible(false);
        //}
        //}
    }
    void setLayout(container_layout ctrLayoutNew) {
        if (this->ctrLayout == ctrLayoutNew) {
            return;
        }
        this->ctrLayout = ctrLayoutNew;
        updateVisible();
        updateSplitters();
    }
    void postContentChanged() override {
        simplify();
        updateVisible();
        if (this->parent) {
            layout();
        }
    }
    void setActiveEntry(int32_t idx) {
        this->activePosition = idx;
        updateVisible();
        if (this->parent) {
            layout();
        }
    }
    void updateSplitters();
    void updateVisible();

    void onChildLayoutChanged(guibase* g) override {
        //postContentChanged();
        if (this->parent) {
            this->parent->onChildLayoutChanged(g);
        } else {
            if (this->parentCtrl) {
                this->parentCtrl->relayout();
            }
        }
    }
    void layout() override;
    void buttonClicked(guibase* button) override {
        if (this->ctrLayout == container_layout::TABBED) {
            int32_t pos = 0;
            for (auto& entry: entries) {
                if (entry->getHandle() == button) {
                    setActiveEntry(pos);
                    return;
                }
                pos++;
            }
        }
    }

    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    container_layout getLayout() const override {
        return this->ctrLayout;
    }
    bool getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) override;
    void addEntry(std::shared_ptr<guictr_layout_entry> ctr, int32_t posOffset = -2);
    bool placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area) override;
    bool activateEntry(guictr_layout_entry* entry) override;

    void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& handles) override;
    //void replaceContentWith(guictr_layout* ctr);

    std::shared_ptr<guictr_layout_entry> replaceContainerWith(guictr_base* ctr, std::shared_ptr<guictr_layout_entry>& newEntry) override;
    void render(NVGcontext* vg) override;
    void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override;
    ivec2 getContainerSize() override;
};



void loadContainerSnapshot(ContainerFactory& fac,
                            ContainerInstanceContext& ctxt,
                            guictr_layout* ctrlayout,
                            guictrlayout_snapshot_t* snapshot);
void storeContainerSnapshot(guictr_layout* ctrlayout, guictrlayout_snapshot_t* snapshot);


