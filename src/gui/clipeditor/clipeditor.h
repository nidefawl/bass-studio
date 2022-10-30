#pragma once
#include <list>
#include <vector>
#include "grid_constants.h"
#include "gui/controls/splitter.h"
#include "gui/dropdown/dropdown.h"
#include "gui/dropdown/dropdown_generic.h"
#include "gui/shape/shapeeditor.h"
#include "guiconstant.h"
#include "logging.h"
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

class i_ctr_shape_editor;

class action_modify_notes : public action_base {
protected:
public:
    int32_t trackIdx = 0;
    tick_t clipTime  = 0;
    clip_notes_t before;
    clip_notes_t after;
    clip_cursor_t cursorBefore;
    clip_cursor_t cursorAfter;
    action_modify_notes() : action_base() {
    }
    //desc, clip, notesBefore, cursorBefore
    action_modify_notes(String description, const clip_view& view, const clip_notes_t& oldNotes, const clip_cursor_t& oldCursor) : action_base() {
        desc = std::move(description);
        //    clip = view.clip;
        after       = view.clip()->notes;
        trackIdx    = view.track()->projectIdx;
        clipTime    = view.clip()->time;
        cursorAfter = view.cursor;
        before      = oldNotes;

        std::list<note_t*> selcopy;
        for (note_t* sel : before.selection) {
            selcopy.insert(selcopy.end(), sel);
        }
#ifndef NDEBUG
        for (note_t* sel : selcopy) {
            bool found = false;
            for (note_t& ent : before.m_list) {
                if (sel == &ent) {
                    found = true;
                    break;
                }
            }
            dbgassert(found);
        }
#endif
        cursorBefore = oldCursor;
        before.removeDuplicates();
        after.removeDuplicates();
    }
    void undo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_midi_t& midi = tr->getMidi();
        clip_t* clip           = midi.getClipAt(clipTime);
        if (!clip)
            return;
        clip->notes = before;
        clip->setDirty();
        daw->updateClipViews(clip, cursorBefore);
    }
    void redo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_midi_t& midi = tr->getMidi();
        clip_t* clip           = midi.getClipAt(clipTime);
        if (!clip)
            return;
        clip->notes = after;
        clip->setDirty();
        daw->updateClipViews(clip, cursorAfter);
    }
};

class action_modify_clip : public action_base {
protected:
public:
    int32_t trackIdx = 0;
    tick_t clipTime  = 0;
    clip_t before;
    clip_t after;
    clip_cursor_t cursorBefore;
    clip_cursor_t cursorAfter;
    action_modify_clip() : action_base() {
    }
    //desc, clip, notesBefore, cursorBefore
    action_modify_clip(String description, const clip_view& view, const clip_t& oldC, const clip_cursor_t& oldCursor) : action_base() {
        desc = description;
        //        clip = view.clip;
        after        = *view.clip();
        trackIdx     = view.track()->projectIdx;
        clipTime     = view.clip()->time;
        cursorAfter  = view.cursor;
        before       = oldC;
        cursorBefore = oldCursor;
    }
    void undo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_midi_t& midi = tr->getMidi();
        clip_t* clip           = midi.getClipAt(clipTime);
        if (!clip)
            return;
        *clip = before;
        clip->setDirty();
        daw->updateClipViews(clip, cursorBefore);
    }
    void redo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_midi_t& midi = tr->getMidi();
        clip_t* clip           = midi.getClipAt(clipTime);
        if (!clip)
            return;
        *clip = after;
        clip->setDirty();
        daw->updateClipViews(clip, cursorAfter);
    }
};

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
    static const int32_t MAX_OCTAVES = (8 - (-2));
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
            }
            return this->clipview.unfoldNote(note);
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
    void setOffset(float f);
    void setScale(float f);
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

class gui_quantizationsettings : public guictr_base {
    tick_t tickStart = 0;
    tick_t tickEnd = 0;
    gui_timeinput inputStarts;
    gui_timeinput inputEnds;
    guibutton btnQuantize;
public:
    gui_quantizationsettings()
        : guictr_base(),
        inputStarts(&tickStart, true),
        inputEnds(&tickEnd, true)
    {
        setLabel("Quantize");
        setBackgroundRendered(true);
        setBackgroundRenderedInset(true);
        setFlag(FLG_RENDER_LABEL, true);
        inputStarts.setLabel("Start");
        inputEnds.setLabel("End");
        btnQuantize.setText("Quantize");
        add(&inputStarts);
        add(&inputEnds);
        add(&btnQuantize);
    }
    ~gui_quantizationsettings() override {
        removeGuis();
    }
    void layout() override {
        padding = theme->get(GuiConstant::CONST_PADDING_EDITOR_CONTROLS);
        const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

        int32_t w    = getSizeContent().x;
        int32_t btnW = (w-padding*3)/2;
        int32_t btnH = TRACK_HEIGHT_STEP;
        int32_t btnX = padding + btnW + padding;
        inputStarts.size = ivec2(btnW, btnH);
        inputEnds.size   = ivec2(btnW, btnH);
        btnQuantize.size   = ivec2(w-padding*2, btnH);
        inputStarts.pos  = ivec2(btnX, 0);
        inputEnds.pos    = ivec2(btnX, btnH + padding);
        btnQuantize.pos    = ivec2(padding, (btnH*2 + padding*2));
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (guibase* gui: guis) {
            nvgSave(vg);
            gui->render(vg);
            nvgRestore(vg);
        }

        const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        nvgSave(vg);
        nvgTranslate(vg, 0, 0);
        for (guibase* gui: guis) {
            renderText(vg, vec2(padding, gui->top() + gui->size.y * 0.5f), vec2(gui->left()-padding, size.y), gui->label, TRACK_HEIGHT_STEP);
        }
        nvgRestore(vg);
    }
    void buttonClicked(guibase* button) override;
    void setQuantization(tick_t start, tick_t end) {
        tickStart = start;
        tickEnd   = end;
    }
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
    gui_numberinput_i32 clipTimeStartOffsedSamples;
    gui_numberinput_i32 clipAudioId;
    guibutton btnDuplicateLoop;
    guibutton btnSelectMuted;
    gui_quantizationsettings quantization;
    gui_clipsettings(scaled_grid& _grid, clip_view& _view);
    ~gui_clipsettings() override;
    void render(NVGcontext* vg) override;

    void layout() override;
    void renderBackground(NVGcontext* vg) override;
    void buttonClicked(guibase* button) override;
    void showEditClip();
};
class gui_clipcontent_base : public guictr_base {
public:
    scaled_grid& grid;
    clip_view& view;
public:
    gui_clipcontent_base(scaled_grid& _grid, clip_view& _view)
        : guictr_base(),
          grid(_grid),
          view(_view) {
        padding = 0;
    }
    void renderBackground(NVGcontext* vg) override;
};
class gui_clipcontent : public gui_clipcontent_base, public piano_scale {
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
    // const bool isVelocity;
    gui_clipcontent(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout, bool _isVel)
        : gui_clipcontent_base(_grid, _view), piano_scale(_layout, _view, size.y)
        //   ,isVelocity(_isVel) 
    {
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
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);


protected:
    void setGlobalSelectionFromClipSelection();
};

class gui_clipcontent_notes : public gui_clipcontent {
public:
    gui_clipcontent_notes(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout) : gui_clipcontent(_grid, _view, _layout, false) {
        setGuiType(gui_type::CTR_TYPE_CLIPEDITOR_NOTES);
    }
    void render(NVGcontext* vg) override;
};

class gui_clipcontent_velocities : public gui_clipcontent {
public:
    gui_clipcontent_velocities(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout) : gui_clipcontent(_grid, _view, _layout, true) {
        setGuiType(gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY);
    }
    void render(NVGcontext* vg) override;
};

struct scaled_pos_t {
    vec2 shapePos;
    vec2 shapeScale;
    vec2 scaledMouse;
};
class CCEdit : public DAW::Shape::ShapeEdit {
    scaled_grid& grid;
    ivec2 editorSize{};
public:
    CCEdit(scaled_grid& _grid)
        : ShapeEdit(),
        grid(_grid) {
        bIsGridEnabledH = true;
        bIsGridEnabledV = true;
        gridStepsV = 2;
    }
    vec2 toParentSpace(const vec2& ctrlPt) const override {
        auto scaledPt = vec2{ ctrlPt.x, 1.0f - ctrlPt.y };
        return editorScale * scaledPt;
    }

    vec2 toNormalizedSpace(const vec2& pt) const override {
        const auto shapePos        = vec2(grid.tickToScreenD(0), 0);
        const auto mouseNormalized = (pt - shapePos) / vec2(editorScale);
        const auto ticksToPx        = grid.tickLenToScreen(1.0);
        return vec2{ pt.x / ticksToPx, 1.0 - mouseNormalized.y };
    }
    
    void layoutEditor(ivec2 size) override {
        editorSize = size;
        editorScale = vec2(grid.tickLenToScreen(1.0), size.y);
    }
    float snapH(float x) override {
        tick_t snappedTick = grid.tickSnapExact(math::roundfS32(x), SNAP_ON);
        return snappedTick;
    }
    float snapV(float y) override {
        return math::roundfS32(y * this->gridStepsV) / float(this->gridStepsV);
    }
};
class gui_clipcontent_control_data : public gui_clipcontent {
    CCEdit shapeEdit;
    DAW::Shape::shape_t tmpShape;
    int32_t cc = 0;
    bool bIsDraggingShape = false;
public:
    gui_clipcontent_control_data(scaled_grid& _grid, clip_view& _view, layout_pianoroll_t& _layout);
    ~gui_clipcontent_control_data() override;
    void render(NVGcontext* vg) override;
    void layout() override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void handleRightClick(MouseEvent& evt) override;
    void showEditClip();
    void setSelectedData(int32_t cc);
    int32_t getSelectedData() const { return cc; }
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

class guictr_noteeditor 
    : public guictr_base,
    public layout_pianoroll_t,
    grid_changed_cb,
    ce_constants,
    splitter_cb
{
public:
    scaled_grid grid;
    gui_pianoroll piano;
    gui_clipcontent_notes content;
    gui_clipcontent_velocities velocities;
    gui_clipcontent_control_data ctrlData;
    guitrack_timeline timeline;
    guictr_cliphandles clipHandles;
    clip_view& view;
    guibuttonstate btnToggleFold;
    guibuttonstate btnToggleVelocities;
    guibuttonstate btnToggleControlData;
    guidropdown_generic<String> dropdownSelectControlData;
    int32_t velHeight = 120;
    int32_t pianoWidth = 100;
    Splitter splitterVel;

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
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt) {
        return content.handleEditorCommand(ctxt);
    }
    void gridChanged(scaled_grid& _grid) override;
    void showEditClip();
    void storeLayout();
    const scaled_grid& getGrid() const {
        return grid;
    }
    void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override;
    ivec2 getContainerSize() override {
        return size;
    }
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
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);

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
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt) {
        return content.handleEditorCommand(ctxt);
    }

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
    explicit guictr_clipeditor(clip_view& _view);
    ~guictr_clipeditor() override;
    void storeLayout();
    void showEditClip();
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void render(NVGcontext* vg) override;
    void layout() override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);
};


class guictr_clipeditorview : public guictr_base {
    clip_view& view;
    midi_clip_render_cache_t* const cache;
    int dragDirection      = -1;
    enum dragmode {
        drag_none,
        drag_view
    };
    dragmode dragMode = drag_none;
public:
    guictr_noteeditor& noteeditor;
    scaled_grid& grid;

    guictr_clipeditorview(clip_view& _view, guictr_noteeditor& _noteeditor);
    ~guictr_clipeditorview();
    void prerender(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    float getScaleX();
    float getScreenSpaceScaleX();
    void getFrameBounds(vec2& posFrame, vec2& sizeFrame);
    void resetCache();

    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    // void layout() override {
    // }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
};
