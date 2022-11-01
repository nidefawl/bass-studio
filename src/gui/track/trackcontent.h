#pragma once
#include <nanovg.h>
#include <vector>
#include "gui/shape/shapeeditor.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "seq_util.h"
#include "color_util.h"
#include "track.h"
#include "clip.h"
#include "grid.h"
#include "gui/container/container.h"
#include "automation.h"
#include "gui/automation/automatable.h"
#include "trackautomation.h"
#include "gui/cliprenderer/cliprenderer.h"
#include "gui/track/trackcontrols.h"
#include "wave/waveform_render.h"

bool getClipPositionInt(scaled_grid& grid, const ivec2& trackSize, const clip_t* cl, ivec2& pos, ivec2& size, double tickOffset, const float minWidth = 2.0f);
bool getClipPositionFloat(scaled_grid& grid, const ivec2& scissorSize, const clip_t* cl, vec2& pos, vec2& size, double tickOffset, const float minWidth = 2.0f);
bool getClippedPosSize(const ivec2& parentSize, ivec2& posClipped, ivec2& sizeClipped);

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

    bool isClipTitleBar(ivec2 mpos, int32_t heightTitle) {
        return mpos.x >= pos.x &&
               mpos.y >= pos.y &&
               mpos.x < pos.x + size.x &&
               mpos.y < pos.y + heightTitle;
    }
    bool isLeftDragZone(ivec2 mpos, int32_t heightTitle) {
        return mpos.x >= pos.x &&
               mpos.y >= pos.y &&
               mpos.x < pos.x + DRAG_RANGE &&
               mpos.y < pos.y + heightTitle;
    }
    bool isRightDragZone(ivec2 mpos, int32_t heightTitle) {
        return mpos.x >= pos.x + size.x - DRAG_RANGE &&
               mpos.y >= pos.y &&
               mpos.x < pos.x + size.x &&
               mpos.y < pos.y + heightTitle;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (culled) {
            return false;
        }
        const auto heightTitle = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        if (isLeftDragZone(mpos, heightTitle)) {
            if (evt.type <= MouseHitType::MOUSE_RIGHT)
                evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
            evt.requestFocus(this);
            return true;
        }
        if (isRightDragZone(mpos, heightTitle)) {
            if (evt.type <= MouseHitType::MOUSE_RIGHT)
                evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
            evt.requestFocus(this);
            return true;
        }
        if (isClipTitleBar(mpos, heightTitle)) {
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

    virtual int getClipType()                    = 0;
    virtual void renderDebugPass(NVGcontext* vg) = 0;
    virtual void updateClipRenderCache(NVGcontext* vg) = 0;
    virtual void updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) = 0;
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
    void renderDebugPass(NVGcontext* vg) override;
    void onRemove() override;
    void handleRightClick(MouseEvent& evt) override;
    void updateClipRenderCache(NVGcontext* vg) override;
};
class rendered_audio_clip_t {
    waveformrender* const waveformRenderer;
    audioclip_texture_t prevWaveform;
    audioclip_texture_t updatedWaveform;
    gui_waveform_texture_ref* waveformRef;
    gui_waveform_texture_ref* tempWaveformRef;
    bool prevIsValid = false;
protected:
    bool bRequestRefresh = false;
    public:
    rendered_audio_clip_t(waveformrender* waveformRenderer);
    virtual ~rendered_audio_clip_t();
    waveformrender* getWaveformRenderer() {
        return waveformRenderer;
    }
    void updateClipPrerender(NVGcontext* vg, clip_t* clip, audiofile_t* audio, bool culled);
    gui_waveform_texture_ref* getWaveformTextureRef();
    const audioclip_texture_t& getCurrentWaveformShape();
    void updateWaveformTexture(const audioclip_texture_t& newShape);
    void releaseWaveformTexture();
};
class gui_audio_clip : public gui_clip, public rendered_audio_clip_t {
public:
    struct fade_layout_t {
        vec2 pos;
        vec2 size;
        sample_fades_ref_t fade;
    };
private:
    fade_layout_t fadeInLayout;
    fade_layout_t fadeOutLayout;
    struct edit_state_t {
        DAW::Shape::ShapeEdit shapeEdit;
        clip_audio_t dataBefore;
    };
    std::unique_ptr<edit_state_t> editState;
    uint8_t editingFade = 0;
    fade_layout_t& getFadeLayout(bool output) {
        return !output ? fadeInLayout : fadeOutLayout;
    }
public:
    gui_audio_clip(track_gui_entry_t* _track, clip_t* _clip, waveformrender* _waveformRenderer);
    ~gui_audio_clip() override;
    DAW::Shape::ShapeEdit& createShapeEdit();
    DAW::Shape::ShapeEdit* getShapeEdit();

    int getClipType() override {
        return CLIP_AUDIO;
    }

    void updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) override;
    void updateClipRenderCache(NVGcontext* vg) override;
    void prerender(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void renderDebugPass(NVGcontext* vg) override;
    void onIdle() override;
    void onTick(AppCtrl* appctrl) override;
    guictxtmenu_base* getTooltip(AppCtrl* appctrl) override;
    void onRemove() override;
    void handleRightClick(MouseEvent& evt) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    vec2 ctrlPtToView(const vec2& ctrlPt, vec2 size) {
        auto scaledPt = vec2{ctrlPt.x, 1.0f - ctrlPt.y};
        return size * scaledPt;
    }
    vec2 viewToCtrlPt(const vec2& pt, vec2 size) {
        vec2 ctrlPt = vec2(pt / size);
        return vec2{ctrlPt.x, 1.0f - ctrlPt.y};
    }
    void handleDraggedBegin(MouseEvent& evt) override;

    void handleDraggedMove(MouseEvent& evt) override;

    void handleDraggedRelease(MouseEvent& evt) override;
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
    //tracklayout_settings_t layout;
    int32_t height = 4;
    int32_t idx    = -1;
    gui_track_subtrack(track_gui_entry_t* _entry, scaled_grid& _grid, automatable_t* _at, int32_t _param);
    //TODO: prefix with get
    virtual int subtrackType() { return SUBTRACK_TYPE_EMPTY; }
    automated_param_t* getAutomation() const {
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
        dawCtrl->getDaw()->setSelectedTrack(m_track);
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
        if (dawCtrl->getDaw()->getSelectedTrack() == m_track) {
            nvgBeginPath(vg);
            nvgRect(vg, pos.x, pos.y, size.x, size.y);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_SELECTEDTRACK));
            nvgFill(vg);
        }
        nvgSave(vg);
        guiTrAutomation.render(vg);
        nvgRestore(vg);
    }
    virtual void renderMixerInfo(NVGcontext* vg, ivec2 pos, ivec2 size);
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (guiTrAutomation.mouseHitTest(mpos, evt)) {
            return true;
        }
        if (this->contains(mpos)) {
            ivec2 localMouse = this->toContainerSpace(mpos);
            for (guibase* gui : guis) {
                if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
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
        //automation.setParent(this->parent);
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

    virtual void renderDebugPass(NVGcontext* vg) {
    }
};
class gui_track_automationlane : public gui_track_subtrack {
public:
    gui_track_automationlane(track_gui_entry_t* _entry, scaled_grid& _grid, automatable_t* _at, int32_t _param);
    int subtrackType() override { return gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION; }
    ~gui_track_automationlane() override = default;
    void handleRightClick(MouseEvent& evt) override;
};

class gui_track : public gui_track_content_base {
protected:
    gui_track_automation automation;
    int subtrackIdx = -1;

public:
    gui_track(track_gui_entry_t* _entry, scaled_grid& _grid);
    ~gui_track() override = default;

    void handleRightClick(MouseEvent& evt) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void prerender(NVGcontext* vg) override;
    void renderDebugPass(NVGcontext* vg);
    void renderTrackFolded(NVGcontext* vg);
    void renderTrack(NVGcontext* vg);
    void render(NVGcontext* vg) override;
    virtual void updateVisibleTrackContents(project_globals_t& project, scaled_grid& grid);

    bool isStaticContainer() override {
        return false;
    }

    bool handleKeyInput(KeyEvent& kevt) override {
        return parent->handleKeyInput(kevt);
    }

    void handleDraggedBegin(MouseEvent& evt) override {
        dawCtrl->getDaw()->setSelectedTrack(m_track);
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
