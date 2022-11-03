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
    String getLayoutCtrName();
    Splitter* getSplitter(int32_t pos);
    bool bHideHandlesWhenLocked = false;
public:
    guictr_layout();
    ~guictr_layout() override {
        removeGuis();
        entries.clear();
    }

    ivec2 paddingTL(int _padding) const override;
    ivec2 paddingBR(int _padding) const override;
    void render(NVGcontext* vg) override;
    void onChildLayoutChanged(guibase* g) override;
    void layout() override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void buttonClicked(guibase* button) override;

    int32_t getActivePosition() const {
        return activePosition;
    }
    std::vector<std::shared_ptr<guictr_layout_entry>>& getEntries() {
        return entries;
    }
    container_layout getLayout() const override {
        return this->ctrLayout;
    }

    void setHideHandlesWhenLocked(bool b) { bHideHandlesWhenLocked = b; }
    bool isHandleShown() const;
    std::vector<float> getSplitterPositions();
    void setSplitterPositions(std::vector<float>& splitterPositons);
    void simplify();
    void setLayout(container_layout ctrLayoutNew);
    void postContentChanged() override;
    void removeAllEntries();
    void assertEntries() const;
    void setActiveEntry(int32_t idx);
    void updateSplitters();
    void updateVisible();

    bool getContainerRef(guictr_layout_entry* ctr, std::shared_ptr<guictr_layout_entry>& out, bool remove) override;
    void addEntry(std::shared_ptr<guictr_layout_entry> ctr, int32_t posOffset = -2);
    bool placeContainer(std::shared_ptr<guictr_layout_entry> ctr, i_ctr_drop_area* area) override;
    bool activateEntry(guictr_layout_entry* entry) override;
    void getOverlays(MouseEvent& evt, std::vector<std::weak_ptr<i_ctr_drop_area>>& handles) override;
    std::shared_ptr<guictr_layout_entry> replaceContainerWith(guictr_base* ctr, std::shared_ptr<guictr_layout_entry>& newEntry) override;
    
    void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override;
    ivec2 getContainerSize() override;
};



void loadContainerSnapshot(ContainerFactory& fac,
                            ContainerInstanceContext& ctxt,
                            guictr_layout* ctrlayout,
                            guictrlayout_snapshot_t* snapshot);
void storeContainerSnapshot(guictr_layout* ctrlayout, guictrlayout_snapshot_t* snapshot);


