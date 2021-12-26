#pragma once
#include <vector>
#include "math/vec.h"
#include "math/seq_math.h"
#include "seq_util.h"
#include "color_util.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "guicontainer.h"
#include "automation.h"
#include "automatable.h"
#include "trackautomation.h"
#include "audiowaveform.h"
#include "cliprenderer.h"

struct gui_waveform_texture_ref;
class guictxtmenu_base;
class gui_clip : public guibase {
public:
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;
    clip_t* const m_clip;
    bool culled = true;
    gui_clip(track_gui_entry_t* _entry, clip_t* _clip)
        : guibase(),
          m_track(_entry->track),
          m_trackentry(_entry),
          m_clip(_clip) {
    }
    ~gui_clip() override = default;

    bool isClipTitleBar(ivec2 mpos) {
        return mpos.x >= pos.x &&
               mpos.y >= pos.y &&
               mpos.x < pos.x + size.x &&
               mpos.y < pos.y + HEIGHT_CLIP_TITLE;
    }
    bool isLeftDragZone(ivec2 mpos) {
        return mpos.x >= pos.x &&
               mpos.y >= pos.y &&
               mpos.x < pos.x + DRAG_RANGE &&
               mpos.y < pos.y + HEIGHT_CLIP_TITLE;
    }
    bool isRightDragZone(ivec2 mpos) {
        return mpos.x >= pos.x + size.x - DRAG_RANGE &&
               mpos.y >= pos.y &&
               mpos.x < pos.x + size.x &&
               mpos.y < pos.y + HEIGHT_CLIP_TITLE;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (culled) {
            return false;
        }
        if (isLeftDragZone(mpos)) {
            if (evt.type <= MouseHitType::MOUSE_RIGHT)
                evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
            evt.requestFocus(this);
            return true;
        }
        if (isRightDragZone(mpos)) {
            if (evt.type <= MouseHitType::MOUSE_RIGHT)
                evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
            evt.requestFocus(this);
            return true;
        }
        if (isClipTitleBar(mpos)) {
            evt.requestFocus(this);
            return true;
        }
        return false;
    }
    bool handleKeyInput(KeyEvent& kevt) override {
        return parent->handleKeyInput(kevt);
    }
    void handleDraggedBegin(MouseEvent& evt) override {
        evt.relMousepos += pos;
        parent->handleDraggedBegin(evt);
    }

    void handleDraggedMove(MouseEvent& evt) override {
        evt.relMousepos += pos;
        parent->handleDraggedMove(evt);
    }

    void handleDraggedRelease(MouseEvent& evt) override {
        evt.relMousepos += pos;
        parent->handleDraggedRelease(evt);
    }

    void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) override;
    void trackViewDragMove(guitrack_editor* view, MouseEvent& evt) override;
    void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) override;
    bool isDragMoveable() override {
        return true;
    }
    virtual int getClipType()                                                                    = 0;
    virtual void updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) = 0;
    virtual void updateClipRenderCache(NVGcontext* vg)                                           = 0;
};
struct midi_clip_render_cache_t;
class gui_midi_clip : public gui_clip {
    midi_clip_render_cache_t* const impl;

public:
    gui_midi_clip(track_gui_entry_t* _track, clip_t* _clip);
    ~gui_midi_clip() override;

    int getClipType() override {
        return CLIP_MIDI;
    }

    void updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) override;
    void prerender(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void onRemove() override;
    void handleRightClick(MouseEvent& evt) override;
    void updateClipRenderCache(NVGcontext* vg) override;
};
class gui_audio_clip : public gui_clip {
    audioclip_texture_t updatedWaveform;
    gui_waveform_texture_ref* waveformRef;

public:
    gui_audio_clip(track_gui_entry_t* _track, clip_t* _clip);
    ~gui_audio_clip() override;

    int getClipType() override {
        return CLIP_AUDIO;
    }

    void updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) override;
    void updateClipRenderCache(NVGcontext* vg) override;
    void prerender(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void releaseRendered();
    void onIdle() override;
    void onTick(AppCtrl* appctrl) override;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    void onRemove() override;
    void handleRightClick(MouseEvent& evt) override;
};


class gui_track_subtrack : public guictr_base {
public:
    static constexpr int SUBTRACK_TYPE_EMPTY      = 0;
    static constexpr int SUBTRACK_TYPE_AUTOMATION = 1;
    static constexpr int SUBTRACK_TYPE_WAVE       = 2;

public:
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;

protected:
    gui_track_automation guiTrAutomation;

public:
    automatable_t* at;
    int32_t param;
    //	tracklayout_settings_t layout;
    int32_t height = 4;
    int32_t idx    = -1;
    gui_track_subtrack(track_gui_entry_t* _entry, scaled_grid& _grid, automatable_t* _at, int32_t _param);
    //TODO: prefix with get
    virtual int subtrackType() { return SUBTRACK_TYPE_EMPTY; }
    automation_t* getAutomation() const {
        if (at) {
            return at->getRegisteredAutomation(param);
        }
        return nullptr;
    }
    void handleRightClick(MouseEvent& evt) override;
    virtual void updateVisibleTrackContents(scaled_grid& grid);
    bool handleKeyInput(KeyEvent& kevt) override {
        return parent->handleKeyInput(kevt);
    }
    bool isStaticContainer() override {
        return false;
    }
    void handleDraggedBegin(MouseEvent& evt) override {
        DawInstance::get()->setSelectedTrack(m_track);
        evt.relMousepos += getPosContent();
        parent->handleDraggedBegin(evt);
    }

    void handleDraggedMove(MouseEvent& evt) override {
        evt.relMousepos += getPosContent();
        parent->handleDraggedMove(evt);
    }

    void handleDraggedRelease(MouseEvent& evt) override {
        evt.relMousepos += getPosContent();
        parent->handleDraggedRelease(evt);
    }

    void render(NVGcontext* vg) override {
        if (DawInstance::get()->getSelectedTrack() == m_track) {
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, pos.y, size.x, size.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_SELECTEDTRACK));
            nvgFill(vg);
        }
        nvgSave(vg);
        guiTrAutomation.render(vg);
        nvgRestore(vg);
    }
    virtual void renderMixerInfo(NVGcontext* vg);
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (guiTrAutomation.mouseHitTest(mpos, evt)) {
            return true;
        }
        if (this->contains(mpos)) {
            ivec2 localMouse = this->toContainerSpace(mpos);
            for (guibase* gui : guis) {
                if (gui->mouseHitTest(localMouse, evt)) {
                    return true;
                }
            }
            if (evt.type == MouseHitType::MOUSE_RIGHT) {// righclick in selection (create clip etc.)
                scaled_grid& grid = m_trackentry->parentCtrl->getGrid();
                tick_t tick       = grid.screenToTickSnap(mpos.x, SNAP_OFF);
                if (m_trackentry->parentCtrl->getCursor().contains(this->m_trackentry->idx, tick)) {
                    evt.requestFocus(this);
                    return true;
                }
            }
            // tracks need to always cancel further mouse tests for z-order to work in parent container
            return true;
        }
        return false;
    }
    void positionChanged() {
        guiTrAutomation.pos  = this->pos;
        guiTrAutomation.size = this->size;
    }
    void setParent(guibase* parent) override {
        guictr_base::setParent(parent);
        //		automation.setParent(this->parent);
        guiTrAutomation.setParent(this->parent);
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        guiTrAutomation.setControl(parentCtrl);
    }
    void layout() override {
        positionChanged();
        guiTrAutomation.layout();
    }
    void destroyGuis() override {
        guiTrAutomation.destroyGuis();
        guictr_base::destroyGuis();
    }
    virtual void updatePosition(const project_globals_t& globals, scaled_grid& grid, ivec2& trackSize, bool throttleRefresh) {
    }
};
class gui_track_automationlane : public gui_track_subtrack {
public:
    gui_track_automationlane(track_gui_entry_t* _entry, scaled_grid& _grid, automatable_t* _at, int32_t _param);
    int subtrackType() override { return gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION; }
    ~gui_track_automationlane() override = default;
    void handleRightClick(MouseEvent& evt) override;
};

class gui_track : public guictr_base {
protected:
    track_t* const m_track;
    track_gui_entry_t* const m_trackentry;
    gui_track_automation automation;
    int subtrackIdx = -1;

public:
    gui_track(track_gui_entry_t* _entry, scaled_grid& _grid);
    ~gui_track() override = default;
    bool isStaticContainer() override {
        return false;
    }

    void handleRightClick(MouseEvent& evt) override;

    bool handleKeyInput(KeyEvent& kevt) override {
        return parent->handleKeyInput(kevt);
    }
    void handleDraggedBegin(MouseEvent& evt) override {
        DawInstance::get()->setSelectedTrack(m_track);
        evt.relMousepos += getPosContent();
        parent->handleDraggedBegin(evt);
    }

    void handleDraggedMove(MouseEvent& evt) override {
        evt.relMousepos += getPosContent();
        parent->handleDraggedMove(evt);
    }

    void handleDraggedRelease(MouseEvent& evt) override {
        evt.relMousepos += getPosContent();
        parent->handleDraggedRelease(evt);
    }

    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;

    void prerender(NVGcontext* vg) override;

    void render(NVGcontext* vg) override {
        if (DawInstance::get()->getSelectedTrack() == m_track) {
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, pos.y, size.x, size.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_SELECTEDTRACK));
            nvgFill(vg);
        }
        nvgSave(vg);
        if (setScissorTransform(vg)) {
            for (auto& entry : m_trackentry->clipsGuis) {
                guibase* gui = entry.second;
                if (!gui) {
                    continue;
                }
                gui->render(vg);
            }
        }
        nvgRestore(vg);
        nvgSave(vg);
        automation.render(vg);
        nvgRestore(vg);
    }

    virtual void updateVisibleTrackContents(project_globals_t& project, scaled_grid& grid);

    void layout() override {
        positionChanged();
        automation.layout();
    }
    void positionChanged() {
        automation.pos  = this->pos;
        automation.size = this->size;
    }
    void setParent(guibase* parent) override {
        guictr_base::setParent(parent);
        automation.setParent(this->parent);
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictr_base::setControl(parentCtrl);
        automation.setControl(parentCtrl);
    }
    void destroyGuis() override {
        automation.destroyGuis();
        guictr_base::destroyGuis();
    }
    track_t* getTrack() {
        return this->m_track;
    }
    track_gui_entry_t* getTrackEntry() {
        return this->m_trackentry;
    }
};
