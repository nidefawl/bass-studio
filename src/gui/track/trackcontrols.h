#pragma once
#include "types.h"
#include "gui/container/container.h"
#include "host/track/track.h"
#include "host/track/trackctr_types.h"
#include "trackctr.h"
#include "host/daw/mainctrl.h"

enum class DragModeTrack : uint8_t {
    DRAG_TRACK_NONE,
    DRAG_TRACK_RESIZE,
};

class gui_track_content_base : public guictr_base {
    scaled_grid& m_grid;
public:
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;
    explicit gui_track_content_base(track_gui_entry_t* _entry, scaled_grid& _grid);
    scaled_grid& getGrid() {
        return m_grid;
    }
};

class gui_track_subtrack_mixer;
class gui_trackcontrols_title;

class gui_track_controls final : public gui_track_content_base {
    gui_trackcontrols_title* title;
    guictr_base* mixer;
    guictr_base* io;
    std::vector<gui_track_subtrack_mixer*> automationLaneControls;
    DragModeTrack dragMode = DragModeTrack::DRAG_TRACK_NONE;

public:
    explicit gui_track_controls(track_gui_entry_t* _entry, scaled_grid& _grid);
    ~gui_track_controls() override;
    void addSubtrackMixer(track_gui_entry_t* entry, gui_track_subtrack* al);
    void removeSubtrackMixer(gui_track_subtrack* al);
    void removeAllAutomationLanes(automatable_t* at, int32_t paramIdx);
    void removeAllAutomationLanes(automatable_t* at);
    void removeAllSubtracks();
    void render(NVGcontext* vg) override;
    void renderGroupHandle(NVGcontext* vg);
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void handleRightClick(MouseEvent& evt) override;
    bool isResize(ivec2 mpos);
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void layout() override;
    guibase* getTitle();
    String getLabel() const override;
};
