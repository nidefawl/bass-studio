#pragma once
#include <vector>
#include "seq_time.h"
#include "theme.h"
#include "guicolors.h"
#include "track.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "grid.h"
#include "trackctr_types.h"
// #include "automation.h"

class gui_track_clipfades : public guictr_base {
protected:
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;

private:

    scaled_grid& grid;
    // automatable_t*& at;
    int32_t& subtrackIdx;
public:
    gui_track_clipfades(track_gui_entry_t* _entry, scaled_grid& _grid, int32_t& _idx)
        : guictr_base(), m_track(_entry->track), m_trackentry(_entry), grid(_grid), subtrackIdx(_idx) {
        padding = 8;
    }
    ivec2 paddingTL(int _padding) const override {
        return {0, _padding};
    }
    ivec2 paddingBR(int _padding) const override {
        return {0, _padding};
    }
    void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) override;
    void trackViewDragMove(guitrack_editor* view, MouseEvent& evt) override;
    void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) override;
    bool trackViewDoubleClick(guitrack_editor* view, MouseEvent& evt) override;
    void postEdit();
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;

    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;

    void updateVisibleTrackContents(scaled_grid& grid);
    void layout() override {
    }

    bool handleKeyInput(KeyEvent& kevt) override {
        return parent->handleKeyInput(kevt);
    }
    void render(NVGcontext* vg) override;
};
