#pragma once
#include <list>
#include <vector>
#include "math/vec.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "color_util.h"
#include "clip.h"
#include "track.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/track/tracktimeline.h"
#include "gui/controls/button.h"
#include "gui/track/trackcontent.h"
#include "gui/views/controls.h"
#include "note.h"
#include "grid.h"
#include "keyboard.h"
#include "edithistory.h"
#include "host/mainctrl.h"
#include "gui/arp/arp.h"
#include "gui/controls/inputfield.h"

#define MAX_OCTAVES (8 - (-2))
#define PIANOROLL_MIN_SCALE 4
#define PIANOROLL_MAX_SCALE 48
inline bool isSharp(int n) {
    n = n % 12;
    switch (n) {
        case 1:
        case 3:
        case 6:
        case 8:
        case 10:
            return true;
    }
    return false;
}

class piano_scale {
private:
    int& sizeY;
    clip_view& clipview;

protected:
    layout_pianoroll_t& layoutRoll;

public:
    piano_scale(layout_pianoroll_t& _layout, clip_view& _clipview, int& _sizeY)
        : sizeY(_sizeY),
          clipview(_clipview),
          layoutRoll(_layout) {
    }
    float toNoteFNoFolding(int32_t y) {
        int32_t rel     = (sizeY - 1) - y;
        float offsetKey = rel + layoutRoll.offset();
        float note      = offsetKey / layoutRoll.scale();
        return note;
    }
    float toNoteFImpl(int32_t y, const bool clamp) {
        int32_t rel     = (sizeY - 1) - y;
        float offsetKey = rel + layoutRoll.offset();
        float note      = offsetKey / layoutRoll.scale();
        if (layoutRoll.fold) {
            if (clamp) {
                return this->clipview.unfoldNoteClamped(note);
            } else {
                return this->clipview.unfoldNote(note);
            }
        }
        return note;
    }
    float toNoteFClamped(int32_t y) {
        return toNoteFImpl(y, true);
    }
    float toNoteF(int32_t y) {
        return toNoteFImpl(y, false);
    }
    float toScreenF(float note) const {
        if (layoutRoll.fold) {
            note = this->clipview.toFoldNote(note);
        }
        float offsetKey = note * layoutRoll.scale();
        float rel       = offsetKey - layoutRoll.offset();
        return (sizeY) -rel;
    }
    void setOffset(float f) {
        auto minOffset            = -(layoutRoll.scale() * MAX_OCTAVES * 1);
        auto maxOffset            = layoutRoll.scale() * (MAX_OCTAVES - 1) * 12;
        this->layoutRoll.offset() = math::clamp(f, minOffset, maxOffset);
    }
    void setScale(float f) {
        this->layoutRoll.scale() = math::clamp<float>(f, PIANOROLL_MIN_SCALE, PIANOROLL_MAX_SCALE);
    }
    void showRange(int32_t noteFrom, int32_t noteTo) {
        noteTo++;
        int32_t nNotes   = math::abs(noteFrom - noteTo);
        float rangeScale = sizeY / (float) nNotes;
        setScale(rangeScale);//TODO: maybe only zoom out here, not in (or determine on upper level)
        setOffset(math::min(noteFrom, noteTo) * layoutRoll.scale());
    }
    void makeNoteVisible(int32_t noteFrom) {
        float foldNote   = layoutRoll.fold ? this->clipview.toFoldNote(noteFrom) : noteFrom;
        float offsetNote = foldNote * layoutRoll.scale();
        if (offsetNote < layoutRoll.offset()) {// below visible area
            setOffset(offsetNote);
        } else if (offsetNote > layoutRoll.offset() + sizeY - layoutRoll.scale()) {
            setOffset(offsetNote - (sizeY - layoutRoll.scale()));
        }
    }
};
class gui_pianoroll : public guibase, public piano_scale {
    enum class dragmode {
        drag_none,
        drag_move_resize,
        drag_piano_key,
    };
    float keysX, widthKeys;
    ivec2 startDrag;
    int dragDirection     = -1;
    float dragPosObjSpace = 0;
    clip_view& view;
    dragmode dragMode    = dragmode::drag_none;
    int32_t lastNote     = -1;
    int32_t lastNoteTime = -1;

public:
    gui_pianoroll(clip_view& _view, layout_pianoroll_t& _layout);
    ~gui_pianoroll() override = default;
    void render(NVGcontext* vg) override;

    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void handleRightClick(MouseEvent& evt) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void layout() override;
    vec2 getNoteFromPos(vec2 pos);
};

class gui_clipsettings : public guictr_base {
public:
    //scaled_grid& grid;
    clip_view& view;
    guibuttonstate btnLoop;
    gui_timeinput clipLoopStart;
    gui_timeinput clipLoopLen;
    gui_timeinput clipTimeStart;
    gui_timeinput clipTimeLen;
    gui_timeinput clipTimeStartOffsetTicks;
    gui_numberinput_field clipTimeStartOffsedSamples;
    gui_numberinput_field clipAudioId;
    guibutton btnDuplicateLoop;
    guibutton btnSelectMuted;
    gui_clipsettings(scaled_grid& _grid, clip_view& _view);
    ~gui_clipsettings() override;
    void render(NVGcontext* vg) override;

    void layout() override;
    void renderBackground(NVGcontext* vg) override;
    void buttonClicked(guibase* button) override;
    void showEditClip();
};

class gui_clipcontent : public guictr_base, public piano_scale {
public:
    enum dragmode {
        drag_none,
        drag_frame,
        drag_notes_move,
        drag_notes_copy,
        drag_note_left,
        drag_note_right,
        drag_velocity,
    };
    clip_cursor_t dragStartCursor;
    dragmode dragMode = drag_none;
    std::set<note_t*> selectionStart;
    ivec2 dragBegin = ivec2(0);
    ivec2 dragTo    = ivec2(0);
    note_t beginDragNote;
    scaled_grid& grid;
    clip_view& view;
    const bool isVelocity;
    gui_clipcontent(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout, bool _isVel)
        : guictr_base(), piano_scale(_layout, _view, size.y),
          grid(_grid),
          view(_view),
          isVelocity(_isVel) {
        padding = 0;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void setStatusText();
    void expandSelectionFrame(std::pair<note_t*, note_t*> minMax);
    void setSelectionFrame(std::pair<note_t*, note_t*> minMax);
    void mergeDraggedNotes(dragmode mergeMode);
    void handleRightClick(MouseEvent& evt) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;

    void layout() override {
        for (guibase* gui : guis) {
            gui->layout();
        }
    }

protected:
    void setGlobalSelectionFromClipSelection();
};

class gui_clipcontent_notes : public gui_clipcontent {
public:
    gui_clipcontent_notes(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout) : gui_clipcontent(_grid, _view, _layout, false) {
    }
    void render(NVGcontext* vg) override;
};

class gui_clipcontent_velocities : public gui_clipcontent {
public:
    gui_clipcontent_velocities(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout) : gui_clipcontent(_grid, _view, _layout, true) {
    }
    void render(NVGcontext* vg) override;
};

class ce_constants {
protected:
    const int32_t heightTimeLine       = 26;
    const int32_t heightSelIndicator   = 8;
    const int32_t heightLoopInidicator = 14;
    const int32_t heightClipIndicators = heightSelIndicator + heightLoopInidicator * 2;
};

class guictr_cliphandles : public guibase, ce_constants {
    scaled_grid& grid;
    clip_view& view;
    enum dragmode {
        drag_handle_none,
        drag_handle_left,
        drag_handle_right,
        drag_handle_loopleft,
        drag_handle_loopright,
        drag_handle_loopbar
    };
    dragmode dragHandle = drag_handle_none;

public:
    ivec2 clipViewSize{0, 0};
    int32_t dragOffset = 0;

public:
    guictr_cliphandles(scaled_grid& _grid, clip_view& _view) : guibase(), grid(_grid), view(_view) {
    }
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    float dist(float x, float y, ivec2 mpos) {
        x = x - mpos.x;
        y = y - mpos.y;
        return x * x + y * y;
    }
    dragmode getDragZone(ivec2 local);
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    float clipStartScrX();
    float clipEndScrX();
    float clipLoopStartScrX() {
        return (float) grid.tickToScreenD(view.clip()->loopStart);
    }
    float clipLoopEndScrX() {
        return (float) grid.tickToScreenD(view.clip()->loopStart + view.clip()->loopLen);
    }
    void render(NVGcontext* vg) override;
};

class guictr_noteeditor : public guictr_base, public layout_pianoroll_t, grid_changed_cb, ce_constants {
public:
    scaled_grid grid;
    gui_pianoroll piano;
    gui_clipcontent_notes content;
    gui_clipcontent_velocities velocities;
    guitrack_timeline timeline;
    guictr_cliphandles clipHandles;
    clip_view& view;
    guibuttonstate btnToggleFold;
    int32_t velHeight = 120;

private:
    void setLayout(layout_pianoroll_t& layout);
    void zoomPianoRollToClipsNoteRange();

public:
    guictr_noteeditor(clip_view& _view);
    ~guictr_noteeditor() override;

    void buttonClicked(guibase* button) override;
    void renderBackground(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void handleDraggedBegin(MouseEvent& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;

    void gridChanged(scaled_grid& _grid) override;

    int32_t getTotalWidth();
    void showEditClip();
    void storeLayout();
};

class gui_audiocontent : public guictr_base {
public:
    scaled_grid& grid;
    clip_view& view;

private:
    audioclip_texture_t updatedWaveform;
    gui_waveform_texture_ref* waveformRef;
    int32_t tickOffset  = 0;
    void renderAudioClip(NVGcontext* vg);

public:
    gui_audiocontent(scaled_grid& _grid, clip_view& _view);
    ~gui_audiocontent() override;
    void layout() override;
    void onTick(AppCtrl* appctrl) override;
    void render(NVGcontext* vg) override;
    void prerender(NVGcontext* vg) override;

    void releaseRendered();
    void updatePosition();
    //protected:
    //void setGlobalSelectionFromClipSelection();
};
class guictr_audioeditor : public guictr_base, grid_changed_cb, ce_constants {
public:
    scaled_grid grid;
    gui_audiocontent content;
    guitrack_timeline timeline;
    guictr_cliphandles clipHandles;
    clip_view& view;

private:
public:
    guictr_audioeditor(clip_view& _view);
    ~guictr_audioeditor() override;

    void buttonClicked(guibase* button) override;
    void renderBackground(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void handleDraggedBegin(MouseEvent& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;

    void gridChanged(scaled_grid& _grid) override;

    int32_t getTotalWidth();
    void showEditClip();
    void storeLayout();
};
class guictr_clipeditor : public guictr_base {
    clip_view& view;

public:
    guictr_noteeditor noteeditor;
    guictr_audioeditor audioeditor;
    gui_clipsettings settings;
    gui_arp arp;
    guictr_clipeditor(clip_view& _view)
        : guictr_base(),
          view(_view),
          noteeditor(view),
          audioeditor(view),
          settings(noteeditor.grid, _view),
          arp(_view) {
        setBackgroundRendered(true);
        setBackgroundRenderedInset(false);
        add(&noteeditor);
        add(&audioeditor);
        add(&arp);
        add(&settings);
    }
    ~guictr_clipeditor() override {
        remove(&settings);
        remove(&arp);
        remove(&audioeditor);
        remove(&noteeditor);
    }
    void storeLayout() {
        const clip_t* clip = view.clip();
        const bool isMidi  = clip && clip->clipType == CLIP_MIDI;
        if (isMidi) {
            noteeditor.storeLayout();
        } else {
            audioeditor.storeLayout();
        }
    }
    void showEditClip() {
        const clip_t* clip = view.clip();
        const bool isMidi  = clip && clip->clipType == CLIP_MIDI;
        arp.setVisible(isMidi);
        noteeditor.setVisible(isMidi);
        audioeditor.setVisible(!isMidi);
        settings.showEditClip();
        if (isMidi) {
            noteeditor.showEditClip();
            arp.showEditClip();
        } else {
            audioeditor.showEditClip();
        }
        layout();
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (!view.clip()) return false;
        return guictr_base::mouseHitTest(mpos, evt);
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        //guictr_base::setScissorTransform(vg);
        ivec2 posInset = getPosContent();
        nvgTranslate(vg, posInset.x, posInset.y);
        if (view.clip()) {
            nvgSave(vg);
            settings.render(vg);
            nvgRestore(vg);
            if (arp.isVisible()) {
                nvgSave(vg);
                arp.render(vg);
                nvgRestore(vg);
            }
            if (noteeditor.isVisible()) {
                noteeditor.render(vg);
            }
            if (audioeditor.isVisible()) {
                audioeditor.render(vg);
            }
        } else {
            auto cs = vec2(getSizeContent());
            renderText(vg, cs * 0.5f, size, "No clip selected", 18, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        }
        for (guibase* gui : guis) {
            if (gui == &audioeditor)
                continue;
            if (gui == &noteeditor)
                continue;
            if (gui == &settings)
                continue;
            if (gui == &arp)
                continue;
            gui->render(vg);
        }
        //nvgResetScissor(vg);
        nvgResetTransform(vg);
    }
    void layout() override {
        const int32_t CONST_LAYOUT_MARGIN = theme->get(GuiConstant::CONST_LAYOUT_MARGIN);
        ivec2 cs      = getSizeContent();
        settings.pos  = ivec2(0, 0);
        settings.size = ivec2(250, cs.y);

        guibase* leftContainer = &settings;
        if (arp.isVisible()) {
            leftContainer = &arp;
            arp.pos       = ivec2(settings.right() + CONST_LAYOUT_MARGIN, 0);
            arp.size      = ivec2(250, cs.y);
        }

        noteeditor.pos   = ivec2(leftContainer->right() + CONST_LAYOUT_MARGIN, 0);
        noteeditor.size  = ivec2(cs.x - leftContainer->right(), cs.y);
        audioeditor.pos  = ivec2(leftContainer->right() + CONST_LAYOUT_MARGIN, 0);
        audioeditor.size = ivec2(cs.x - leftContainer->right(), cs.y);

        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    bool handleKeyInput(KeyEvent& kevt) override {
        if (audioeditor.isVisible()) {
            return audioeditor.handleKeyInput(kevt);
        }
        return noteeditor.handleKeyInput(kevt);
    }
};


class guictr_clipeditorview : public guictr_base {
    midi_clip_render_cache_t* const cache;
public:
    guictr_noteeditor& noteeditor;
    guictr_clipeditorview(guictr_noteeditor& _noteeditor);
    ~guictr_clipeditorview();
    void prerender(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    vec2 getScale();

    void handleDraggedBegin(MouseEvent& evt) override {
        if (evt.guiDragged == this) {
            MainCtrl::get()->showClipEditor();
            //lastscrolloffset = noteeditor.scrolloffset;
        }
    }
    void handleDraggedMove(MouseEvent& evt) override {
        if (evt.guiDragged == this) {
            //ivec2 move = evt.mousepos - evt.dragStart;
            //vec2 scale = getScale();
            //float minScale = min(scale.x, scale.y);
            //noteeditor.setScrolloffset(lastscrolloffset + (int)(move.x*(1.0 / minScale)));
        }
    }
    void layout() override {
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos)) {
            evt.requestFocus(this);
            return true;
        }
        return false;
    }
};
