#pragma once
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>
#include "assert_dbg.h"
#include "basectrl.h"
#include "grid_constants.h"
#include "gui/controls/splitter.h"
#include "gui/dropdown/dropdown.h"
#include "gui/dropdown/dropdown_generic.h"
#include "gui/shape/shapeeditor.h"
#include "guiconstant.h"
#include "layout.h"
#include "logging.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "color_util.h"
#include "host/clip/clip.h"
#include "host/track/track.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/track/tracktimeline.h"
#include "gui/controls/button.h"
#include "gui/track/trackcontent.h"
#include "gui/views/controls.h"
#include "note.h"
#include "grid.h"
#include "keyboard.h"
#include "host/daw/edithistory.h"
#include "host/daw/mainctrl.h"
#include "gui/arp/arp.h"
#include "gui/controls/inputfield.h"
#include "gui/container/scrollcontainer.h"
#include "theme.h"
#include "types.h"
#include "gui/clipeditor/clipeditor_python_processor.h"

class i_ctr_shape_editor;

class action_modify_notes final : public action_base {
    struct clip_changed_t {
        int32_t trackIdx = 0;
        tick_t clipTime  = 0;
        clip_notes_t before;
        clip_notes_t after;
        clip_cursor_t cursorBefore;
        clip_cursor_t cursorAfter;
        bool bHasCursor = true;
    };
public:
    std::vector<clip_changed_t> clipsChanged;
    action_modify_notes() : action_base() {
    }

    action_modify_notes(String description, clip_view_t& view, const clip_cursor_t& oldCursor) : action_base() {
        desc = std::move(description);
        if (view.isAbsoluteTimeMode()) {
            view.visitClipViewTracks([&](track_t* track, std::vector<clip_t*>& clips) {
                for (clip_t* clip : clips) {
                    auto& notesBefore = view.m_notesDragged[clip].dragStartNotes;
                    if (notesBefore != clip->notes) {
                        clipsChanged.push_back({track->projectIdx, clip->time, notesBefore, clip->notes, oldCursor, view.m_cursor});
                    }
                }
                return true;
            });
        } else {
            auto clip = view.clip();
            auto& notesBefore = view.m_notesDragged[clip].dragStartNotes;
            if (notesBefore != clip->notes) {
                clipsChanged.push_back({view.track()->projectIdx, clip->time, notesBefore, clip->notes, oldCursor, view.m_cursor});
            }
        }
    }
    void undo(DawInstance* daw) override {
        for (auto& clipChanged : clipsChanged) {
            track_t* tr = daw->getTracks()[clipChanged.trackIdx];
            if (!tr)
                continue;
            trackdata_clips_t& midi = tr->getClips();
            clip_t* clip           = midi.getClipAt(clipChanged.clipTime);
            if (!clip)
                continue;
            clip->notes = clipChanged.before;
            clip->setDirty();
            if (clipChanged.bHasCursor)
                daw->updateClipViewsAndCursor(clip, clipChanged.cursorBefore);
            else
                daw->updateClipViews(clip);
        }
    }
    void redo(DawInstance* daw) override {
        for (auto& clipChanged : clipsChanged) {
            track_t* tr = daw->getTracks()[clipChanged.trackIdx];
            if (!tr)
                continue;
            trackdata_clips_t& midi = tr->getClips();
            clip_t* clip           = midi.getClipAt(clipChanged.clipTime);
            if (!clip)
                continue;
            clip->notes = clipChanged.after;
            clip->setDirty();
            if (clipChanged.bHasCursor)
                daw->updateClipViewsAndCursor(clip, clipChanged.cursorAfter);
            else
                daw->updateClipViews(clip);
        }
    }
};

class action_modify_clip final : public action_base {
protected:
public:
    int32_t trackIdx = 0;
    tick_t clipTime  = 0;
    clip_t before;
    clip_t after;
    clip_cursor_t cursorBefore;
    clip_cursor_t cursorAfter;
    bool bHasCursor = false;
    action_modify_clip() : action_base() {
    }
    //desc, clip, notesBefore, cursorBefore
    action_modify_clip(String description, const clip_view_t& view, const clip_t& oldC, const clip_cursor_t& oldCursor) : action_base() {
        desc = description;
        //        clip = view.clip;
        after        = *view.clip();
        trackIdx     = view.track()->projectIdx;
        clipTime     = view.clip()->time;
        cursorAfter  = view.m_cursor;
        before       = oldC;
        cursorBefore = oldCursor;
        bHasCursor   = true;
    }
    action_modify_clip(String description, const track_t* track, const clip_t& oldC, const clip_t* newC) : action_base() {
        desc = description;
        //        clip = view.clip;
        after        = *newC;
        trackIdx     = track->projectIdx;
        clipTime     = newC->time;
        before       = oldC;
        bHasCursor   = false;
    }
    void undo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_clips_t& midi = tr->getClips();
        clip_t* clip = midi.getClipAt(clipTime);
        if (!clip)
            return;
        *clip = before;
        clip->setDirty();
        if (bHasCursor)
            daw->updateClipViewsAndCursor(clip, cursorBefore);
        else
            daw->updateClipViews(clip);
    }
    void redo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_clips_t& midi = tr->getClips();
        clip_t* clip = midi.getClipAt(clipTime);
        if (!clip)
            return;
        *clip = after;
        clip->setDirty();
        if (bHasCursor)
            daw->updateClipViewsAndCursor(clip, cursorAfter);
        else
            daw->updateClipViews(clip);
    }
};

class action_modify_clip_control_data final : public action_base {
public:
    int32_t trackIdx = 0;
    tick_t clipTime  = 0;
    clip_control_data_t before;
    clip_control_data_t after;
    clip_cursor_t cursorBefore;
    clip_cursor_t cursorAfter;
    bool bHasCursor = false;
    action_modify_clip_control_data() : action_base() {
    }
    action_modify_clip_control_data(String description, const clip_view_t& view, const clip_control_data_t& oldC, const clip_cursor_t& oldCursor) : action_base() {
        desc = description;
        //        clip = view.clip;
        after        = view.clip()->controlData;
        trackIdx     = view.track()->projectIdx;
        clipTime     = view.clip()->time;
        cursorAfter  = view.m_cursor;
        before       = oldC;
        cursorBefore = oldCursor;
    }
    void undo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_clips_t& midi = tr->getClips();
        clip_t* clip           = midi.getClipAt(clipTime);
        if (!clip)
            return;
        clip->controlData = before;
        clip->controlData.updateBounds();
        clip->setDirty();
        if (bHasCursor)
            daw->updateClipViewsAndCursor(clip, cursorBefore);
        else
            daw->updateClipViews(clip);
    }
    void redo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_clips_t& midi = tr->getClips();
        clip_t* clip           = midi.getClipAt(clipTime);
        if (!clip)
            return;
        clip->controlData = after;
        clip->controlData.updateBounds();
        clip->setDirty();
        if (bHasCursor)
            daw->updateClipViewsAndCursor(clip, cursorAfter);
        else
            daw->updateClipViews(clip);
    }
};

class action_modify_clip_groove_setting final : public action_base {
public:
    int32_t trackIdx = 0;
    tick_t clipTime  = 0;
    int32_t before;
    int32_t after;
    action_modify_clip_groove_setting() : action_base() {
    }
    action_modify_clip_groove_setting(String description, const clip_view_t& view, const int32_t& oldC) : action_base() {
        desc = description;
        after        = view.clip()->selectedGroove;
        trackIdx     = view.track()->projectIdx;
        clipTime     = view.clip()->time;
        before       = oldC;
    }
    void undo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_clips_t& midi = tr->getClips();
        clip_t* clip           = midi.getClipAt(clipTime);
        if (!clip)
            return;
        clip->selectedGroove = before;
        clip->setDirty();
        daw->updateClipViews(clip);
    }
    void redo(DawInstance* daw) override {
        track_t* tr = daw->getTracks()[trackIdx];
        if (!tr)
            return;
        trackdata_clips_t& midi = tr->getClips();
        clip_t* clip           = midi.getClipAt(clipTime);
        if (!clip)
            return;
        clip->selectedGroove = after;
        clip->setDirty();
        daw->updateClipViews(clip);
    }
};

class action_modify_groove_data final : public action_base {
public:
    groove_data_t before;
    groove_data_t after;
    action_modify_groove_data() : action_base() {
    }
    action_modify_groove_data(String description, const project_t& project, const groove_data_t& oldD, const groove_data_t& newD) : action_base() {
        desc = description;
        before       = oldD;
        after        = newD;
    }
    void undo(DawInstance* daw) override {
        auto& grooves = daw->getGrooves();
        auto it = std::find_if(grooves.begin(), grooves.end(), [&](const groove_data_t& g) {
            return g.presetName == before.presetName;
        });
        if (it != grooves.end()) {
            *it = before;
        }
    }
    void redo(DawInstance* daw) override {
        auto& grooves = daw->getGrooves();
        auto it = std::find_if(grooves.begin(), grooves.end(), [&](const groove_data_t& g) {
            return g.presetName == before.presetName;
        });
        if (it != grooves.end()) {
            *it = after;
        }
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
    clip_view_t& clipview;

protected:
    layout_pianoroll_t& layoutRoll;

public:
    static const int32_t MAX_OCTAVES = (8 - (-2));
    piano_scale(layout_pianoroll_t& _layout, clip_view_t& _clipview, int& _sizeY)
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
        if (layoutRoll.bFoldNotes) {
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
        if (layoutRoll.bFoldNotes) {
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
    void makeNotePitchVisible(int32_t noteFrom) {
        float foldNote   = layoutRoll.bFoldNotes ? this->clipview.toFoldNote(noteFrom) : noteFrom;
        float offsetNote = foldNote * layoutRoll.scale();
        if (offsetNote < layoutRoll.offset()) {// below visible area
            setOffset(offsetNote);
        } else if (offsetNote > layoutRoll.offset() + sizeY - layoutRoll.scale()) {
            setOffset(offsetNote - (sizeY - layoutRoll.scale()));
        }
    }
};

class gui_pianoroll final : public guibase, public piano_scale {
    enum class dragmode {
        drag_none,
        drag_move_resize,
        drag_piano_key,
    };
    float keysX, widthKeys;
    ivec2 startDrag;
    int dragDirection     = -1;
    float dragPosObjSpace = 0;
    clip_view_t& view;
    dragmode dragMode    = dragmode::drag_none;
    int32_t lastNote     = -1;
    int32_t lastNoteTime = -1;

public:
    gui_pianoroll(clip_view_t& _view, layout_pianoroll_t& _layout);
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

class gui_quantize_clip final : public guictr_base {
    tick_t tickStart = 0;
    tick_t tickEnd = 0;
    gui_timeinput inputStarts;
    gui_timeinput inputEnds;
    guibutton btnQuantize;
public:
    explicit gui_quantize_clip()
        : guictr_base(),
        inputStarts(true),
        inputEnds(true)
    {
        inputStarts.setRef(toRef(), &tickStart);
        inputEnds.setRef(toRef(), &tickEnd);
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
    ~gui_quantize_clip() override {
        removeGuis();
    }
    void layout() override {
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
class gui_clipsettings;
class gui_clipgroove_settings final : public guictr_base {
    gui_clipsettings& parentClipSettings;
public:
    clip_view_t& view;
    gui_timeinput lenQuantization;
    gui_numberinput_field_percentage strengthQuantization;
    gui_numberinput_field_percentage strengthGroove;
    gui_numberinput_field_percentage strengthVelocity;
    gui_numberinput_field_percentage randomTiming;
    gui_numberinput_field_percentage randomVelocity;
    guidropdownbase* dropdownSelectPreset;
    guidropdownbase* dropdownSelectGroove;
    guibutton btnApply;
    groove_data_t grooveData;
public:
    explicit gui_clipgroove_settings(gui_clipsettings& parent, clip_view_t& _view);
    ~gui_clipgroove_settings() override;
    void setSelectedGroove(const int32_t& _selectedGroove);
    void layout() override;
    void render(NVGcontext* vg) override;
    void buttonClicked(guibase* button) override;
};

class gui_clipsettings final : public guictr_base {
    guictr_clipeditor& parentClipEditor;
public:
    clip_view_t& view;
    guibuttonstate btnLoop;
    gui_timeinput clipLoopStart;
    gui_timeinput clipLoopLen;
    gui_timeinput clipTimeStart;
    gui_timeinput clipTimeLen;
    gui_timeinput clipTimeStartOffsetTicks;
    gui_numberinput_i32 clipTimeStartOffsetSamples;
    gui_numberinput_i32 clipAudioId;
    gui_numberinput_float clipAudioPitch;
    gui_numberinput_float clipAudioStretch;
    guibutton btnDuplicateLoop;
    guibutton btnSelectMuted;
    gui_clipgroove_settings grooveSettings;
    gui_quantize_clip quantization;
    std::vector<guictr_base*> noteEditorScripts;
    clip_audio_settings_t clipAudioSettings;
    explicit gui_clipsettings(guictr_clipeditor& parent, clip_view_t& _view);
    ~gui_clipsettings() override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void renderBackground(NVGcontext* vg) override;
    void buttonClicked(guibase* button) override;
    void updateClipViewReferences();
    bool isVisible() const override;
    void determineSize(ivec2& prefSize) override;
};
class gui_clipcontent_base : public guictr_base {
public:
    scaled_grid& grid;
    clip_view_t& view;
public:
    gui_clipcontent_base(scaled_grid& _grid, clip_view_t& _view)
        : guictr_base(),
          grid(_grid),
          view(_view) {
        padding = 0;
    }
    void renderBackground(NVGcontext* vg) override;
    tick_t getTickOffset() const;
};
class gui_clipcontent : public gui_clipcontent_base, public piano_scale {
public:
    enum dragmode {
        drag_none,
        drag_note_clicked,
        drag_frame,
        drag_notes_move,
        drag_notes_copy,
        drag_note_left,
        drag_note_right,
        drag_velocity,
    };
    clip_cursor_t dragStartCursor;
    dragmode dragMode = drag_none;
    std::map<clip_t*, std::set<note_t*>> selectionsStart;
    ivec2 dragBegin = ivec2(0);
    ivec2 dragTo    = ivec2(0);
    int32_t dragBeginPitch = 0;
    tick_t dragBeginTick = 0;
    note_t beginDragNote;
    noteview_render_t notesViewTemp;
    // const bool isVelocity;
    gui_clipcontent(scaled_grid& _grid, clip_view_t& _view, layout_pianoroll_t& _layout, bool _isVel)
        : gui_clipcontent_base(_grid, _view), piano_scale(_layout, _view, size.y)
        //   ,isVelocity(_isVel) 
    {
        padding = 0;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void setStatusText();
    void expandSelectionFrame(std::pair<note_t*, note_t*> minMax);
    void setSelectionFrame(std::pair<note_t*, note_t*> minMax);
    void setSelectionFrameFromView();
    int32_t mergeDraggedNotes(dragmode mergeMode);
    bool mergeDraggedNotes(dragmode mergeMode, clip_t* clip);
    void handleRightClick(MouseEvent& evt) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override {
        if (dragMode >= drag_notes_move) {
            auto local = toContainerSpace(evt.relMousepos);
            auto evtCopy = evt;
            evtCopy.relMousepos = local;
            handleDraggedMove(evtCopy);
        }
        return false;
    }


protected:
    void setGlobalSelectionFromClipSelection();
};

class gui_clipcontent_notes final : public gui_clipcontent {
    void renderClipNoteRects(NVGcontext* vg, const std::vector<note_t>& clipNotes, vec2 renderPos, vec2 renderSize, 
                                tick_t tickOffset, float scale, float inset, NVGcolor color, int32_t shading, bool renderMuted);
    void renderNoteLabels(NVGcontext* vg, const std::vector<note_t>& clipNotes, vec2 renderPos, vec2 renderSize, 
                                tick_t tickOffset, float scale, bool bRenderPosLen, bool bRenderMuted = true);
public:
    gui_clipcontent_notes(scaled_grid& _grid, clip_view_t& _view, layout_pianoroll_t& _layout) : gui_clipcontent(_grid, _view, _layout, false) {
        setGuiType(gui_type::CTR_TYPE_CLIPEDITOR_NOTES);
    }
    void render(NVGcontext* vg) override;
    guibase* getFocusedContainer() override {
        return this;
    }
};

class gui_clipcontent_velocities final : public gui_clipcontent {
public:
    gui_clipcontent_velocities(scaled_grid& _grid, clip_view_t& _view, layout_pianoroll_t& _layout) : gui_clipcontent(_grid, _view, _layout, true) {
        setGuiType(gui_type::CTR_TYPE_CLIPEDITOR_VELOCITY);
    }
    void render(NVGcontext* vg) override;
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
};

struct scaled_pos_t {
    vec2 shapePos;
    vec2 shapeScale;
    vec2 scaledMouse;
};
class CCEdit final : public DAW::Shape::ShapeEdit {
    scaled_grid& grid;
    clip_view_t& view;
    ivec2 editorSize{};
    std::vector<int32_t> selectedNodeIndices;
public:
    CCEdit(scaled_grid& _grid, clip_view_t& _view)
        : ShapeEdit(),
        grid(_grid),
        view(_view) {
        bIsGridEnabledH = true;
        bIsGridEnabledV = true;
        gridStepsV = 2;
    }
    const std::vector<int32_t>& getSelectedNodeIndices() const {
        return selectedNodeIndices;
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
    float snapH(float x) override;
    float snapV(float y) override {
        return math::roundfS32(y * this->gridStepsV) / float(this->gridStepsV);
    }
    void setSelectRect(vec4 rect);
    void resetSelection() {
        selectedNodeIndices.clear();
    }
    void selectAll() {
        selectedNodeIndices.clear();
        auto len = int32_t(curve->pts.size());
        for (int32_t i = 0; i < len; ++i) {
            selectedNodeIndices.push_back(i);
        }
    }
    void deleteSelectedPoints() {
        std::vector<int32_t> indices = selectedNodeIndices;
        curveTmp = *curve;
        if (!indices.empty()) {
            // sort indices
            std::sort(indices.begin(), indices.end());
            // be careful, indices change when deleting.
            for (size_t i = indices.size(); i > 0; --i) {
                curveTmp.pts.erase(curveTmp.pts.begin() + indices[i - 1]);
            }
        }
        selectedNodeIndices.clear();
        callback(curveTmp, true);
    }
};
class gui_clipcontent_control_data final : public gui_clipcontent {
    CCEdit shapeEdit;
    DAW::Shape::shape_t tmpShape;
    int32_t cc = 0;
    bool bIsDraggingShape = false;
    clip_control_data_t controlDataBegin;
public:
    gui_clipcontent_control_data(scaled_grid& _grid, clip_view_t& _view, layout_pianoroll_t& _layout);
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
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);
};

class ce_constants {
protected:
    const int32_t heightTimeLine       = 26;
    const int32_t heightSelIndicator   = 8;
    const int32_t heightLoopInidicator = 14;
    const int32_t heightClipIndicators = heightSelIndicator + heightLoopInidicator * 2;
};

class guictr_editor_base;
class guictr_cliphandles final : public guibase, ce_constants {
public:
    enum dragmode {
        drag_handle_none,
        drag_handle_left,
        drag_handle_right,
        drag_handle_loopleft,
        drag_handle_loopright,
        drag_handle_loopbar
    };
    struct dist_dragzone {
        float dist    = 0;
        dragmode mode = drag_handle_none;
    };
    struct dist_dragzone_handle final : public dist_dragzone {
        guictr_cliphandles* handle = nullptr;
    };
private:
    guictr_editor_base& parentEditor;
    scaled_grid& grid;
    clip_view_t view;
    int32_t trackSelectionIdx = 0;
    bool bIsHandleActive = true;
    dragmode dragModeMouseOver = drag_handle_none;
    dragmode dragHandle = drag_handle_none;
public:
    int32_t dragOffset = 0;
public:
    explicit guictr_cliphandles(guictr_editor_base& parentEditor, scaled_grid& _grid)
        : parentEditor(parentEditor), grid(_grid) {
        setBackgroundRendered(true);
        setGuiType(gui_type::GUI_CLIPEDITOR_CLIPHANDLES);
    }
    void setHandleActive(bool b) {
        bIsHandleActive = b;
    }
    bool isHandleActive() const {
        return bIsHandleActive;
    }
    void setDragMode(dragmode mode) {
        dragModeMouseOver = mode;
    }
    tick_t getTickOffset() const;
    tick_t getTickOffsetOffset() const;
    clip_view_t& getClipView() { return view; }
    const clip_view_t& getClipView() const { return view; }
    void setTrackSelectionIdx(int32_t idx) { trackSelectionIdx = idx; }
    int32_t getTrackSelectionIdx() const { return trackSelectionIdx; }
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    float dist(float x, float y, ivec2 mpos) {
        x = x - mpos.x;
        y = y - mpos.y;
        return x * x + y * y;
    }
    dist_dragzone_handle getDragZone(ivec2 local);
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    float clipStartScrX() const;
    float clipEndScrX() const;
    float clipLoopStartScrX() const;
    float clipLoopEndScrX() const;
    void render(NVGcontext* vg) override;
    void renderHandle(NVGcontext* vg, int32_t trackSelIdx) const;
    void renderLoopHandle(NVGcontext* vg, vec2 editorSize) const;
    bool containsHandlePos(ivec2 mpos) const;
    void setControl(BaseCtrl* parentCtrl) override {
        view.reset();
        guibase::setControl(parentCtrl);
    }
};

void renderClipHandlesBackground(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid, vec2 handlesPos, vec2 handlesSize);
void renderGridList(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid, vec2 handlesPos, vec2 handlesSize);
void renderSelectionIndicator(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid , ivec2 handlesPos, ivec2 handlesSize, const clip_t* viewClip, const DAW::Cursor& c, float heightSelIndicator);
void renderPlayHead(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid, ivec2 handlesPos, ivec2 handlesSize, const clip_t* viewClip, tick_t playbackPos, bool bIsAbsoluteTime, float fWidth);

class guictr_editor_base : public guictr_base, public grid_changed_cb, public ce_constants {
protected:
    guictr_clipeditor& parentClipEditor;
    gui_clipcontent_base* pContent;
    clip_view_t& view;
    scaled_grid m_grid;
    guitrack_timeline timeline;
    std::vector<std::shared_ptr<guictr_cliphandles>> clipsHandles;
    int32_t handlesHeight = heightClipIndicators;
    virtual void zoomPianoRollToClipsNoteRange();
    clip_editor_layout_t lastLayout{};
public:
    explicit guictr_editor_base(guictr_clipeditor& parentClipEditor, gui_clipcontent_base* pContent, clip_view_t& _view)
        : guictr_base(),
          parentClipEditor(parentClipEditor),
          pContent(pContent),
          view(_view),
          timeline(m_grid) {
        setCanMouseHit(true);
        m_grid.setGridMaxDens(6);
    }
    ~guictr_editor_base() override {
        removeGuis();
    }

    scaled_grid& getGrid() {
        return m_grid;
    }
    const scaled_grid& getGrid() const {
        return m_grid;
    }

    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void layout() override;
    
    clip_view_t& getClipView() { return view; }
    virtual ivec2 getContentSize() const = 0;
    guictr_clipeditor& getClipEditor() { return parentClipEditor; }
    virtual void relayout();
    virtual void selectEditClip(clip_t* clip);
    void onClipChanged();
    virtual void storeEditorLayout();
    virtual void renderClipHandles(NVGcontext* vg);
    virtual void updateCopiedClipData() { };
};

class guidropdown_popup_sel_control_data final : public guictxtmenu {
    gui_clipcontent_control_data* const m_ctrlData;
public:
    class ctxt_menu_entry_data final : public ctxtmenu_entry {
        bool m_hasData;
    public:
        ctxt_menu_entry_data(int32_t _id, const String& name, bool automated)
            : ctxtmenu_entry(name, _id),
            m_hasData(automated)
        {
            if (m_hasData) {
                setIcon(&RenderResources::imgIcons[ICON_AUTOMATION], GuiColor::COL_AUTOMATED);
            }
            bGrayedOut = !m_hasData;
            if (_id > 0) {
                this->rightTitle = std::to_string(_id);
            }
        }
        ~ctxt_menu_entry_data() override = default;
        void render(ivec2 ctxtSize, NVGcontext* vg, int idx, ivec2 mouse) override {
            ctxtmenu_entry::render(ctxtSize, vg, idx, mouse);
            
        }
    };
    explicit guidropdown_popup_sel_control_data(DawCtrl* _dawCtrl, gui_clipcontent_control_data* const ctrlData) : m_ctrlData(ctrlData) {
        this->dawCtrl  = _dawCtrl;
        this->size.x   = 120;
        this->fontSize = FONT_SIZE_CTXT_SMALL;
        this->paddingV = 0;

        auto clip = ctrlData->view.clip();
        bool bHasData = false;
        if (clip) {
            bHasData = clip->controlData.pitchBend.hasData();
        }
        addEntry(new ctxt_menu_entry_data(0, "Pitch Bend", bHasData));
        for (int32_t i = 1; i < 127; ++i) {
            String name = IMidiMsg::ControlName(i);
            bHasData = false;
            if (clip) {
                auto it = clip->controlData.ccChannels.find(i);
                if (it != clip->controlData.ccChannels.end()) {
                    bHasData = it->second.hasData();
                }
            }
            addEntry(new ctxt_menu_entry_data(i, name, bHasData));
        }
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        m_ctrlData->setSelectedData(_id);
        closeContextMenu();
        return true;
    }
};

class guidropdown_midi_control_data final : public guidropdownbase {
    gui_clipcontent_control_data* const m_ctrlData;

public:
    explicit guidropdown_midi_control_data(gui_clipcontent_control_data* const ctrlData)
        : guidropdownbase(),
          m_ctrlData(ctrlData) {
    }
    String getString() override {
        auto control = m_ctrlData->getSelectedData();
        if (control == 0) {
            return "Pitch Bend";
        }
        return IMidiMsg::ControlName(control);
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        guictxtmenu_base* popup = new guidropdown_popup_sel_control_data(dawCtrl, m_ctrlData);
        popup->size.x           = 250;
        m_ctrlData->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y)) - popup->pos + ivec2(1));
    }
};

class guictr_clipeditor;
class guictr_noteeditor 
    : public guictr_editor_base,
    public layout_pianoroll_t,
    splitter_cb
{
public:
    gui_pianoroll piano;
    gui_clipcontent_notes content;
    gui_clipcontent_velocities velocities;
    gui_clipcontent_control_data ctrlData;
    guibuttonstate btnShowClipSettings;
    guibuttonstate btnShowArp;
    guibuttonstate btnShowVelocities;
    guibuttonstate btnShowControlData;
    guibuttonstate btnToggleFold;
    guidropdown_midi_control_data dropdownSelectControlData;
    Splitter splitterVel;
    int32_t velHeight = 120;
    int32_t pianoWidth = 100;
    ivec2 posContentArea{ 0, 0 };
    ivec2 sizeContentArea{ 0, 0 };
    std::array<guibase*, 5> buttonList = { &btnShowClipSettings, &btnShowArp, &btnShowVelocities, &btnShowControlData, &btnToggleFold };
    bool bShowControlData = true;
    bool bShowVelocity = true;
protected:
    void setLayout(layout_pianoroll_t& layout);
    void zoomPianoRollToClipsNoteRange() override;
public:
    explicit guictr_noteeditor(guictr_clipeditor& parentClipEditor, clip_view_t& _view);
    ~guictr_noteeditor() override;
    void setControl(BaseCtrl *parentCtrl) override;
    void buttonClicked(guibase* button) override;
    void renderBackground(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    void renderClipHandles(NVGcontext* vg) override;
    void layout() override;
    void handleDraggedBegin(MouseEvent& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt) {
        return content.handleEditorCommand(ctxt);
    }
    void gridChanged(scaled_grid& _grid) override;
    void relayout() override;
    void selectEditClip(clip_t* clip) override;
    void storeEditorLayout() override;
    void updateCopiedClipData() override;
    void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override;
    ivec2 getContainerSize() override {
        return sizeContentArea;
    }
    ivec2 getContentSize() const override {
        return sizeContentArea;
    }
    ivec2 getContainerPos() override {
        return toScreenSpace(posContentArea);
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    guibase* getFocusedContainer() override {
        return content.getFocusedContainer();
    }
};

class gui_audiocontent final : public gui_clipcontent_base {
    audioclip_texture_t updatedWaveform;
    gui_waveform_texture_ref* waveformRef;
    int32_t tickTimerRefresh  = 0;
    void renderAudioClip(NVGcontext* vg);
    clip_dragaction action;
public:
    gui_audiocontent(scaled_grid& _grid, clip_view_t& _view);
    ~gui_audiocontent() override;
    void layout() override;
    void onTick(AppCtrl* appctrl) override;
    void render(NVGcontext* vg) override;
    void prerender(NVGcontext* vg) override;
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);

    bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override {
        bool hit = guictr_base::mouseHitTest(v, evt);
        if (!hit && this->contains(v) && evt.type == MOUSE_DRAGDROP_CLIP) {
            evt.requestFocus(this);
            return true;
        }
        return hit;
    }

    bool clipDropBegin(dragdrop_midifile& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        bool bHasAudioSample = false;
        for (auto& track : clip.clipboard->tracks) {
            for (auto& c : track->clips) {
                if (c->audio.id >= 0) {
                    bHasAudioSample = true;
                    break;
                }
            }
            if (bHasAudioSample)
                break;
        }
        if (bHasAudioSample) {
            clip_clipboard* clipboard = clip.clipboard.get();
            clipboard->srcTrack       = 0;
            action.dragtype           = clip_dragtype_t::DROP_FILE_EXTERNAL;
            action.clipboard          = clip.clipboard;
            action.cursorBegin        = {};
            clip.isValidTarget        = true;//inform higher level that we accept and process this drop attempt
            clip.target               = makeSafeRef();
            return true;
        }
        return false;
    }

    bool clipDropMove(dragdrop_midifile& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        if (!action.dragtype) {
            if (!clipDropBegin(clip, mousepos, kbmods))
                return false;
        }
        if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {
            // dragClipboardMove(mousepos, kbmods);
            clip.isValidTarget = true;//inform higher level that we accept and process this drop attempt
            clip.target        = makeSafeRef();
            return true;
        }
        return false;
    }

    bool clipDropFinal(dragdrop_midifile& clip, ivec2 mousepos, KeyboardMods kbmods) override {
        if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {

            auto thisClip = this->view.clip();
            if (!thisClip)
                return false;
            for (auto& track : clip.clipboard->tracks) {
                for (auto& clip : track->clips) {
                    if (clip->audio.id >= 0) {
                        thisClip->audio.id = clip->audio.id;
                        thisClip->setDirty();
                        dawCtrl->getDaw()->updateVisibleTrackContents();
                        dawCtrl->showClipEditor();
                        dawCtrl->getDaw()->setSingleClip(thisClip);
                        releaseRendered();
                        updatePosition();
                        return true;
                    }
                }
            }
            return true;
        }
        return false;
    }

    void clipDropCancel() override {
        if (action.dragtype == clip_dragtype_t::DROP_FILE_EXTERNAL) {
            action.clipboard = nullptr;
            action.dragtype  = DRAG_NONE;
        }
    }


    void releaseRendered();
    void updatePosition();
    guibase* getFocusedContainer() override {
        return this;
    }
};

class guictr_audioeditor final : public guictr_editor_base {
public:
    gui_audiocontent content;
public:
    explicit guictr_audioeditor(guictr_clipeditor& parentClipEditor, clip_view_t& _view);
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
    void relayout() override;
    void storeEditorLayout() override;
    ivec2 getContentSize() const override {
        return content.size;
    }
    guibase* getFocusedContainer() override {
        return content.getFocusedContainer();
    }
};
class guictr_clipeditor final : public guictr_base {
    clip_view_t view;
public:
    guictr_noteeditor noteeditor;
    guictr_audioeditor audioeditor;
    gui_clipsettings settingsCtr;
    guictr_scrollbar settingsScrollCtr;
    gui_arp arp;
    explicit guictr_clipeditor();
    ~guictr_clipeditor() override;
    guictr_noteeditor& getNoteEditor() {
        return noteeditor;
    }
    guictr_audioeditor& getAudioEditor() {
        return audioeditor;
    }
    void storeEditorLayout();
    void updateClipViewReferences();
    void resetClipView();
    void setSingleClip(clip_t* clip);
    void setEditorSelection(clip_t* clip, const editor_view_selection_t& clipboardView);
    void selectEditClip(clip_t* clip);
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void render(NVGcontext* vg) override;
    void buttonClicked(guibase* button) override;
    void layout() override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);
    clip_view_t& getClipView() {
        return view;
    }
    void setControl(BaseCtrl* parentCtrl) override {
        if (this->parentCtrl)
            resetClipView();
        guictr_base::setControl(parentCtrl);
        if (parentCtrl)
            resetClipView();
    }
    void refreshAudioWaveform();
    guibase* getFocusedContainer() override {
        if (audioeditor.isVisible())
            return audioeditor.getFocusedContainer();
        return noteeditor.getFocusedContainer();
    }
};


class guictr_clipeditorview final : public guictr_base {
    SPLayoutEntry clipEditor;
    scaled_grid m_grid;
    clip_view_t m_view;
    midi_clip_render_cache_t* const cache;
    int dragDirection      = -1;
    enum dragmode {
        drag_none,
        drag_view
    };
    dragmode dragMode = drag_none;
public:

    guictr_clipeditorview();
    ~guictr_clipeditorview() override;
    void setClipEditor(SPLayoutEntry& _clipEditor) {
        clipEditor = _clipEditor;
        auto clipEditor = getClipEditor();
        if (clipEditor) {
            m_grid = clipEditor->noteeditor.getGrid();
        }
    }
    guictr_clipeditor* getClipEditor() {
        if (clipEditor && clipEditor->getType() == gui_type::CTR_TYPE_CLIPEDITOR)
            return guictr_cast<guictr_clipeditor>(clipEditor);
        return nullptr;
    }
    scaled_grid& getGrid() {
        auto clipEditor = getClipEditor();
        if (clipEditor)
            return clipEditor->noteeditor.getGrid();
        return m_grid;
    }
    clip_view_t& getClipView() {
        auto clipEditor = getClipEditor();
        if (clipEditor)
            return clipEditor->getClipView();
        return m_view;
    }
    void prerender(NVGcontext* vg) override;
    void render(NVGcontext* vg) override;
    float getScaleX();
    float getScreenSpaceScaleX();
    void getFrameBounds(vec2& posFrame, vec2& sizeFrame);
    void resetCache();
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
};
