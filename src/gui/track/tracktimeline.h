#pragma once
#include <nanovg.h>
#include "grid.h"
#include "event.h"
#include "gui/container/container.h"

class guitrack_timeline : public guictr_base, grid_changed_cb {
    scaled_grid& grid;

public:
    ivec2 startDrag{ 0, 0 };
    int dragDirection      = -1;
    float dragPosSS = 0.0f;
    double dragPosObjSpace = 0;
    explicit guitrack_timeline(scaled_grid& _grid)
        : guictr_base(),
          grid(_grid) {
        setGuiType(gui_type::CTR_TYPE_TRACKS_TIMELINE);
        setCanMouseHit(true);
        grid.addCallback(this);
        padding = 0;
    }

    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void adjustZoom(float mousePosXScreenSpaceLocal, float disty);
    void adjustOffset(float gridOffset);
    void render(NVGcontext* vg) override;
    void layout() override {
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void gridChanged(scaled_grid& _grid) override {
    }
    guibase* getFocusedControl() override {
        return this->parent;
    }
    guibase* getFocusedContainer() override {
        return this->parent;
    }
};
