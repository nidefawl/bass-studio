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

class guictr_layout : public guictr_base, public guictr_layout_base, public splitter_cb {
    bool setOverlayPos(DropAreaUILayout* area, dock_pos dockPos, ivec2 overlayPos, ivec2 overlaySize, int32_t dockPosOfffset, int32_t childContainerIndex);
    bool setOverlayPosForTab(DropAreaUILayout* area, dock_pos dockPos, int32_t dockOffset, bool rightSideHandle);
    DropAreaUILayout* makeDropArea(int32_t idx);

private:
    container_layout ctrLayout = container_layout::SOLE;
    int32_t activePosition     = -1;
    std::vector<std::shared_ptr<GuiCtrLayoutEntry>> entries;
    std::vector<guibase*> handles;
    std::vector<std::shared_ptr<DropAreaUILayout>> dragdropContainerAreaHelpers;
    std::vector<std::shared_ptr<Splitter>> splitters;
    String getLayoutCtrName();
    Splitter* getSplitter(int32_t pos);
    bool bHideHandlesWhenLocked = false;
    int32_t tag = -1;
public:
    guictr_layout();
    ~guictr_layout() override;
    void setTag(int32_t tag) {
        this->tag = tag;
    }
    int32_t getTag() const {
        return tag;
    }
    template<typename T>
    bool visitEntries(T&& visitor) {
        for (auto& entry : entries) {
            if (!visitor(entry))
                return false;
            auto childCtr = entry->getAsLayoutCtr();
            if (childCtr) {
                if (!childCtr->visitEntries(visitor))
                    return false;
            }
        }
        return true;
    }
    // std::shared_ptr<guictr_layout_entry> findByTagContainer(int32_t tag);
    std::shared_ptr<GuiCtrLayoutEntry> findByTagEntry(int32_t tag);
    std::shared_ptr<GuiCtrLayoutEntry> findByGuiType(gui_type guitype);

    int32_t getActivePosition() const {
        return activePosition;
    }
    std::vector<std::shared_ptr<GuiCtrLayoutEntry>>& getEntries() {
        return entries;
    }

    std::shared_ptr<GuiCtrLayoutEntry> getEntry(guictr_base* ctr) {
        for (auto& entry : entries) {
            if (entry->getGui() == ctr)
                return entry;
        }
        return nullptr;
    }

    container_layout getLayout() const override {
        return this->ctrLayout;
    }

    void setHideHandlesWhenLocked(bool b) { bHideHandlesWhenLocked = b; }
    bool isHandleShown() const;
    bool canSimplify() const;
    void simplify();
    void postContentChanged() override;
    void assertEntries() const;
    void setLayout(container_layout ctrLayoutNew);
    void addEntry(std::shared_ptr<GuiCtrLayoutEntry> ctr, int32_t posOffset = -2);
    void removeAllEntries();
    void setActiveEntry(int32_t idx);
    bool isEntryVisible(GuiCtrLayoutEntry* entry) override;
    bool getContainerRef(GuiCtrLayoutEntry* ctr, std::shared_ptr<GuiCtrLayoutEntry>& out, bool remove) override;
    bool placeContainer(std::shared_ptr<GuiCtrLayoutEntry> ctr, DropAreaUILayout* area) override;
    bool activateEntry(GuiCtrLayoutEntry* entry) override;
    void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<DropAreaUILayout>>& handles) override;
    std::shared_ptr<GuiCtrLayoutEntry> replaceContainerWith(guictr_base* ctr, std::shared_ptr<GuiCtrLayoutEntry>& newEntry) override;
    container_layout DockPosToContainerLayout(dock_pos pos) {
        switch (pos) {
            case dock_pos::TOP:
            case dock_pos::BOTTOM:
                return container_layout::SPLIT_H;
            case dock_pos::LEFT:
            case dock_pos::RIGHT:
                return container_layout::SPLIT_V;
            case dock_pos::STACK:
                return container_layout::TABBED;
            default:
                if (!entries.empty())
                    return this->ctrLayout;
                break;
        }
        return container_layout::SOLE;
    }

private:
    void updateHandles();
    void updateSplitters();
    void updateVisible();

public:
    ivec2 paddingTL(int _padding) const override;
    ivec2 paddingBR(int _padding) const override;
    void render(NVGcontext* vg) override;
    void layout() override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void buttonClicked(guibase* button) override;
    void onChildLayoutChanged(guibase* g) override;
    void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override;
    ivec2 getContainerPos() override;
    ivec2 getContainerSize() override;
    
    std::vector<float> getSplitterPositions();
    void setSplitterPositions(std::vector<float>& splitterPositons);
};



void loadContainerSnapshot(ContainerFactory& fac,
                            ContainerInstanceContext& ctxt,
                            guictr_layout* ctrlayout,
                            guictrlayout_snapshot_t* snapshot);
void storeContainerSnapshot(guictr_layout* ctrlayout, guictrlayout_snapshot_t* snapshot);


