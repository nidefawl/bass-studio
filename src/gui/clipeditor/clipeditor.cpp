

#include "guiglobals.h"
#include "logging.h"
#include "note.h"
#include "seq_time.h"
#include "math/seq_math.h"
#include "cursor.h"
#include "keyboard.h"
#include "grid.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "gui/contextmenu/contextmenu.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/track/trackcontent.h"
#include "clipeditor.h"
#include "gui/cliprenderer/cliprenderer_cache.h"
#include "track.h"
#include "track_impl.h"
#include "host/vst_host.h"
#include "appconfig.h"
#include <nanovg.h>

constexpr int32_t VEL_SELECT_DISTANCE = 16;
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
        desc = description;
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
        for (note_t* sel : selcopy) {
            bool found = false;
            for (note_t& ent : before.m_list) {
                if (sel == &ent) {
                    found = true;
                    break;
                }
            }
            if (!found) dbgassert(0);
        }
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

class guictxtmenu_noteeditor : public guictxtmenu {
    guictr_noteeditor* editor;

public:
    guictxtmenu_noteeditor(guictr_noteeditor* _editor) {
        this->editor  = _editor;
        this->size.x  = 320;
        auto adaptive = new ctxtmenu_time_select(editor->grid, "Adaptive Grid", 0);
        adaptive->initAdaptive();
        addEntry(adaptive);
        auto fixed = new ctxtmenu_time_select(editor->grid, "Fixed Grid", 0);
        fixed->initFixed();
        addEntry(fixed);
    }
    void clicked(int clickedid) override {
        scaled_grid& grid = editor->grid;
        if (clickedid == 110 + 9) {// OFF
            grid.grid_dens.enabled = false;
        } else if (clickedid >= 110) {
            grid.grid_dens.enabled   = true;
            grid.grid_dens.fixedBars = clickedid - 110;
            grid.grid_dens.isfixed   = true;
        } else {
            grid.grid_dens.enabled        = true;
            grid.grid_dens.dynamicDensity = static_cast<int8_t>(math::clamp<int32_t>(clickedid - 100 + 2, 0, 8));
            grid.grid_dens.isfixed        = false;
        }
        grid.notifyChange();
        closeContextMenu();
    }
};

void gui_clipcontent::handleRightClick(MouseEvent& evt) {
    guictr_noteeditor* editor = dynamic_cast<guictr_noteeditor*>(this->parent);
    dawCtrl->openContextMenu(new guictxtmenu_noteeditor(editor), evt.mousepos);
}

template<typename T>
void renderNote(NVGcontext* vg, gui_clipcontent* c, T* note, float yscale, tick_t offset = 0) {

    float ny     = c->toScreenF(note->pitch);
    float nx     = c->grid.tickToScreenD(note->time + offset);
    float nw     = c->grid.tickLenToScreen(note->len);
    float nh     = yscale;
    float insetx = calcInset(1, nw);
    float insety = calcInset(1, nh);
    nvgBatchedRect(vg, nx + insetx, ny - yscale + insety, nw - insetx * 2, nh - insety * 2);
}
void renderNoteName(NVGcontext* vg, const gui_clipcontent* c, note_t* note, int idx, float yscale, tick_t absPos) {

    const float ny     = c->toScreenF(note->pitch);
    const float nx     = c->grid.tickToScreenD(note->time);
    const float nw     = c->grid.tickLenToScreen(note->len);
    const float nh     = yscale;
    const float insetx = calcInset(5, nw);
    const String strNoteName = StringFormat("%s @%d %d", noteName(note->pitch), absPos, note->len);
    renderTextLabel(vg, 
        vec2(nx + insetx, ny - yscale + nh / 2.0f),
        vec2(nw - insetx + 2, nh),
        strNoteName,
        c->theme, 18, c->theme->getColor(GuiColor::COL_NOTE_TEXT), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
}
void renderFrame(NVGcontext* vg, ivec2 posA, ivec2 posB) {
    float x = math::min(posA.x, posB.x);
    float y = math::min(posA.y, posB.y);
    float w = math::max(posA.x - posB.x, posB.x - posA.x);
    float h = math::max(posA.y - posB.y, posB.y - posA.y);
    renderDashedLineFrame(vg, x, y, w, h, 2.0f);
}
void renderGridLines(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid, const ivec2& size) {

    for (grid_div g: grid.gridList) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, g.screenpos, 0);
        nvgLineTo(vg, g.screenpos, size.y);
        NVGcolor col;
        switch (g.color) {
            case 0:
                col = theme->getColor(GuiColor::COL_LINE_BAR);
                break;
            case 1:
                col = theme->getColor(GuiColor::COL_LINE_QRT);
                break;
            case 2:
            default:
                col = theme->getColor(GuiColor::COL_LINE_XTH);
                break;
        }
        nvgStrokeColor(vg, col);
        nvgStrokeWidth(vg, g.thickness);
        nvgStroke(vg);
    }
}

inline int32_t screenToVel(int y, int h) {
    dbgassert(h > 0);
    return (int32_t) ((h - 1 - y) * 127 / h);
}

note_t* getMinDistNoteVel(clip_notes_t& notes, int32_t tickExact, int32_t tickDist, int32_t velExact, int32_t velDist) {
    int32_t minDist     = 0;
    int32_t minDistV    = 0;
    note_t* minDistNote = nullptr;
    auto checkNoteDist  = [&minDistNote, &minDist, &minDistV, tickExact, tickDist, velExact, velDist](note_t& note) {
        int32_t dist  = note.start() - tickExact;
        int32_t distV = note.velocity - velExact;
        if (dist > -tickDist && dist < tickDist && distV > -velDist && distV < velDist) {
            if (minDistNote == nullptr || minDist > math::abs(dist) || minDistV > math::abs(distV)) {
                minDistNote = &note;
                minDist     = math::abs(dist);
                minDistV    = math::abs(distV);
            }
        }
    };
    notes.visitSelection([&checkNoteDist](note_t* pNote) {
        checkNoteDist(*pNote);
    });
    if (minDistNote != nullptr) {
        return minDistNote;
    }
    notes.visitNotes([&checkNoteDist](note_t& note) {
        checkNoteDist(note);
    });
    return minDistNote;
}

void duplicateClipLoop(DawInstance* daw, clip_view& view) {
    clip_t* clip = view.clip();
    if (!clip) {
        return;
    }

    if (clip->loopLen > 0) {
        ThreadLock lock                = daw->lockPlayThread();
        clip_t clipBefore              = *clip;
        clip_notes_t& notes            = clip->notes;
        clip_cursor_t& cursor          = view.cursor;
        clip_cursor_t cursorBefore     = cursor;// copy
        const clip_notes_t notesBefore = notes; // copy


        int32_t loopStart = clip->loopStart;
        int32_t loopEnd   = loopStart + clip->loopLen;
        int32_t offset    = clip->loopLen;
        {
            {
                clip_notes_t notesCopy = notes;// copy
                std::vector<note_t> newNotes;
                std::vector<note_t> selNotes;
                notesCopy.storeSelection(selNotes);
                notesCopy.clearSelection();
                notesCopy.visitNotes([loopStart, loopEnd, offset, &newNotes](note_t& note) {
                    if (note.time >= loopStart && note.time < loopEnd) {
                        note_t noteCpy = note;
                        noteCpy.time += offset;
                        newNotes.push_back(noteCpy);
                    }
                    if (note.time >= loopEnd) {
                        note.time += offset;
                    }
                });
                notesCopy.addAll(newNotes);
                notesCopy.restoreSelection(selNotes);
                notesCopy.updateBounds();

                notes = notesCopy;
            }

            clip->loopLen *= 2;

            String desc = "Duplicate clip loop";
            daw->pushHist(new action_modify_clip(desc, view, clipBefore, cursorBefore));
            clip->setDirty();
            view.updateNotePitches(false);
        }
    }
}
void gui_clipcontent_velocities::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    float w         = (float) size.x;
    double bgRepeat = grid.incr_bg * 2.0;
    float bgOffset  = (float) fmod((double) grid.offset, bgRepeat);
    int steps_bg    = math::ceildS32((w + bgRepeat) / grid.incr_bg);
    float x         = -bgOffset;

    nvgBeginPath(vg);
    nvgRect(vg, -2, 0, w + 2, size.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
    nvgFill(vg);
    for (int i = 0; i < steps_bg; i += 2) {
        nvgBeginPath(vg);
        nvgRect(vg, x, 0, grid.incr_bg, size.y);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
        nvgFill(vg);
        x += grid.incr_bg * 2.0f;
        if (x > w)
            break;
    }

    const float h             = size.y;
    const clip_notes_t& notes = view.clip()->notes;
    NVGpaint paint{};
    paint.image     = -1;
    paint.customPar = 1;
    nvgShapeAntiAlias(vg, 0);
    const float extendCullCheck = 8.0f;
    if (!notes.empty()) {
        const int32_t nw = 4;
        const float r    = 4;
        for (int i = 0; i < 2; i++) {
            int nRendered = 0;
            for (const note_t& note: notes.m_list) {
                if ((i == 0) != note.isEnabled())
                    continue;
                float nx = grid.tickToScreenD(note.time);
                if (nx + nw / 2.0f < -extendCullCheck) continue;
                if (nx - nw / 2.0f > w + extendCullCheck) continue;
                float nh     = velocityToFloat(note.velocity) * h;
                float insetx = calcInset(1, nw);
                float insety = calcInset(1, nh);
                nvgBatchedRect(vg, nx - nw / 2.0f + insetx, size.y - nh + insety, nw - insetx * 2, nh - insety * 2);
                nRendered++;
            }
            if (nRendered) {
                paint.innerColor = theme->getColor(i == 0 ? GuiColor::COL_NOTE : GuiColor::COL_NOTE_MUTE);
                paint.renderType = 4;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);

                for (const note_t& note: notes.m_list) {
                    if ((i == 0) != note.isEnabled())
                        continue;
                    float nx = grid.tickToScreenD(note.time);
                    if (nx + r < -extendCullCheck) continue;
                    if (nx - r > w + extendCullCheck) continue;
                    float nh = velocityToFloat(note.velocity) * h;
                    nvgBatchedRect(vg, nx - r, size.y - nh - r, r * 2, r * 2);
                }
                paint.renderType = 5;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }
    }
    if (!notes.selection.empty()) {
        const int32_t nw = 5;
        const float r    = 5;
        for (int i = 0; i < 2; i++) {
            int nRendered = 0;
            for (const note_t* pnote: notes.selection) {
                if ((i == 0) != pnote->isEnabled())
                    continue;
                float nx = grid.tickToScreenD(pnote->time);
                if (nx + nw / 2.0f < -extendCullCheck) continue;
                if (nx - nw / 2.0f > w + extendCullCheck) continue;
                float nh     = velocityToFloat(pnote->velocity) * h;
                float insetx = calcInset(1, nw);
                float insety = calcInset(1, nh);
                nvgBatchedRect(vg, nx - nw / 2.0f + insetx, size.y - nh + insety, nw - insetx * 2, nh - insety * 2);
                nRendered++;
            }
            if (nRendered) {
                paint.innerColor = theme->getColor(i == 0 ? GuiColor::COL_NOTE_SELECTED : GuiColor::COL_NOTE_MUTE);
                paint.renderType = 4;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
                for (const note_t* pnote: notes.selection) {
                    if ((i == 0) != pnote->isEnabled())
                        continue;
                    float nx = grid.tickToScreenD(pnote->time);
                    if (nx + r < -extendCullCheck) continue;
                    if (nx - r > w + extendCullCheck) continue;
                    float nh = velocityToFloat(pnote->velocity) * h;
                    nvgBatchedRect(vg, nx - r, size.y - nh - r, r * 2, r * 2);
                }
                paint.renderType = 5;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }
    }
    nvgShapeAntiAlias(vg, USE_NANOVG_AA);
    if (dragMode <= drag_frame) {
        const int32_t nw = 6;
        const float r    = 6;
        ivec2 imouse     = toControlsObjectSpace(dawCtrl->m_mousePos, this);
        bool mouseIn     = dawCtrl->guiOver == this && contains(imouse + getPosContent());
        if (mouseIn) {
            tick_t mouseTick = !mouseIn ? INVALID_TICK : grid.screenToTickSnap(imouse.x, SNAP_OFF);
            //    vec2 fmouse = vec2(imouse);
            int32_t velClicked = screenToVel(imouse.y, size.y);
            int32_t velDist    = VEL_SELECT_DISTANCE * 127 / size.y;
            //    const tick_t tickExact = grid.screenToTickSnap(imouse.x, SNAP_OFF);
            note_t* contextNote = getMinDistNoteVel(view.clip()->notes, mouseTick, grid.pixelsToTicks(VEL_SELECT_DISTANCE), velClicked, velDist);
            if (contextNote) {


                nvgBeginPath(vg);
                float nx     = grid.tickToScreenD(contextNote->time);
                float nh     = velocityToFloat(contextNote->velocity) * h;
                float insetx = calcInset(1, nw);
                float insety = calcInset(1, nh);
                nvgRect(vg, nx - nw / 2.0f + insetx, size.y - nh + insety, nw - insetx * 2, nh - insety * 2);

                nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_SELECTED));
                nvgFill(vg);

                nvgBeginPath(vg);
                nvgCircle(vg, nx, size.y - nh, r);

                nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_SELECTED));
                nvgFill(vg);
            }
        }
    }
    //  hit_result currentDragged = dragged.mode || !mouseIn ? dragged : hitTest(fmouse);
    //  if (currentDragged.mode == dragmode::drag_node) {
    //    int32_t ptIdx = currentDragged.dataPt;
    //    dbgassert(ptIdx >= 0 && ptIdx < (int)data.points.size());
    //    automation_point_t& pt = data.points[ptIdx];
    //    vec2* point = getPathPointSafe(currentDragged.segidx);
    //    mouseTick = pt.time;
    //    fmouse.x = point->x;
    //  }

    x = (float) grid.tickToScreenD(view.cursor.start);
    if (view.cursor.start == view.cursor.end) {
        if (x >= -2 && x < size.x + 2) {
            x += 0.5;
            NVGcolor cursorColor = getCursorColor();
            nvgBeginPath(vg);
            nvgMoveTo(vg, x, 1);
            nvgLineTo(vg, x, size.y - 1);
            nvgStrokeColor(vg, cursorColor);
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }
    } else {
        float x2 = (float) grid.tickToScreenD(view.cursor.end);
        if (x2 > -4.0f && x < size.x + 4.0f) {
            float xBegin = CLAMP_I(x, -4.0f, size.x + 3.0f);
            float xEnd   = CLAMP_I(x2, -3.0f, size.x + 4.0f);
            float width  = xEnd - xBegin;
            nvgBeginPath(vg);
            nvgRect(vg, xBegin, -2.0f, width, size.y + 2.0f);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_SELECTION_BACKGROUND));
            nvgFill(vg);
        }
    }
    if (dragMode == drag_frame) {
        renderFrame(vg, dragBegin, dragTo);
    }
}
void gui_clipcontent_notes::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    float w         = (float) size.x;
    double bgRepeat = grid.incr_bg * 2.0;
    float bgOffset  = (float) fmod((double) grid.offset, bgRepeat);
    int steps_bg    = math::ceildS32((w + bgRepeat) / grid.incr_bg);
    float x         = -bgOffset;

    nvgBeginPath(vg);
    nvgRect(vg, -2, 0, w + 2, size.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
    nvgFill(vg);
    for (int i = 0; i < steps_bg; i += 2) {
        nvgBeginPath(vg);
        nvgRect(vg, x, 0, grid.incr_bg, size.y);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
        nvgFill(vg);
        x += grid.incr_bg * 2.0f;
        if (x > w)
            break;
    }

    float h             = size.y;
    clip_notes_t& notes = view.clip()->notes;
    bool fold           = layoutRoll.fold;
    float offset        = layoutRoll.offset();
    float scale         = layoutRoll.scale();
    int32_t firstKey    = math::max((int32_t) floorf(offset / scale), 0);
    //render one extra key on top and bottom to fix antialiasing on edge of container
    if (firstKey > 0) {
        firstKey--;
    }
    float yOff = offset - firstKey * scale - scale;
    if (fold) {
        std::vector<int32_t> pitches;
        this->view.getNotePitches(pitches);


        nvgSave(vg);
        nvgTranslate(vg, 0, yOff);
        float y = 0;
        int numRowsSharp = 0;
        int len = CtrSize(pitches);
        for (int i = firstKey; i < len; i++) {
            int32_t pitch = pitches[i];
            if (isSharp(pitch)) {
                if (numRowsSharp == 0) {
                    nvgBeginPath(vg);
                }
                nvgRect(vg, 0, h - y, w, scale);
                numRowsSharp++;
            }
            y += scale;
            if (y >= size.y + scale * 2) {
                break;
            }
        }
        if (numRowsSharp) {
            nvgFillColor(vg, theme->getColor(GuiColor::COL_CLIPEDITOR_SHARP));
            nvgFill(vg);
        }

        renderGridLines(vg, theme, grid, size);

        //    nvgBeginPath(vg);
        //    nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
        //    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
        //    if (firstKey == 0 && octave == 0) {
        //      nvgMoveTo(vg, 0, h - (y-scale));
        //      nvgLineTo(vg, w, h - (y-scale));
        //    }
        //    nvgStroke(vg);

        nvgBeginPath(vg);
        y = 0;
        for (int i = firstKey; i < len; i++) {
            nvgMoveTo(vg, 0, h - y);
            nvgLineTo(vg, w, h - y);
            y += scale;
            if (y >= size.y + scale * 2) {
                break;
            }
        }
        nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
        nvgStroke(vg);
        nvgRestore(vg);
        //    if (yoct >= size.y+scale*2) {
        //      break;
        //    }
    } else {

        int32_t firstOctave = floorf(firstKey / 12.0f);
        firstKey            = firstKey % 12;

        //    nvgSave(vg);
        //    nvgTranslate(vg, 0, yOff);
        float yoct = 0;
        for (int32_t octave = firstOctave; octave < MAX_OCTAVES; octave++) {
            float y = yoct;
            nvgBeginPath(vg);
            for (int i = firstKey; i < 12; i++) {
                if (isSharp(i)) {
                    nvgRect(vg, 0, h - y + yOff, w, scale);
                }
                y += scale;
                if (y >= size.y + scale * 2) {
                    break;
                }
            }
            nvgFillColor(vg, theme->getColor(GuiColor::COL_CLIPEDITOR_SHARP));
            nvgFill(vg);


            renderGridLines(vg, theme, grid, size);

            y = yoct;
            nvgBeginPath(vg);
            if (firstKey == 0 && octave == 0) {
                nvgMoveTo(vg, 0, h - (y - scale) + yOff);
                nvgLineTo(vg, w, h - (y - scale) + yOff);
            }
            nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
            nvgStroke(vg);

            nvgBeginPath(vg);
            for (int i = firstKey; i < 12; i++) {
                nvgMoveTo(vg, 0, h - y + yOff);
                nvgLineTo(vg, w, h - y + yOff);
                y += scale;
                if (y >= size.y + scale * 2) {
                    break;
                }
            }
            nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
            nvgStroke(vg);
            yoct = y;
            if (yoct >= size.y + scale * 2) {
                break;
            }
            firstKey = 0;
        }
    }

    //  nvgRestore(vg);
    if (!notes.empty()) {
        for (int i = 0; i < 2; i++) {
            int nRendered = 0;
            for (note_t& note: notes.m_list) {
                if ((i == 0) != note.isEnabled())
                    continue;
                float nx = grid.tickToScreenD(note.time);
                float nw = grid.tickLenToScreen(note.len);
                if (nx + nw < -4)
                    continue;
                if (nx > w + 4)
                    continue;
                float ny     = toScreenF(note.pitch);
                float nh     = scale;
                float insetx = calcInset(1, nw);
                float insety = calcInset(1, nh);
                nvgBatchedRect(vg, nx + insetx, ny - scale + insety, nw - insetx * 2, nh - insety * 2);
                nRendered++;
            }
            if (nRendered) {
                auto noteColor = theme->getColor(i == 0 ? GuiColor::COL_NOTE : GuiColor::COL_NOTE_MUTE);
                NVGpaint paint{};
                paint.image      = -1;
                paint.innerColor = noteColor;
                paint.customPar  = 1;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }
    }
    nvgBeginPath(vg);
    int n2 = 0;
    if (dragMode >= drag_notes_move) {
        for (note_t& note: view.draggedSelection) {
            renderNote(vg, this, &note, scale);
            n2++;
        }
    } else {
        for (note_t* pnote: notes.selection) {
            renderNote(vg, this, pnote, scale);
            n2++;
        }
    }
    if (n2) {
        NVGpaint paint{};
        paint.image      = -1;
        paint.innerColor = theme->getColor(GuiColor::COL_NOTE_SELECTED);
        paint.customPar  = 3;
        nvgFillPaint(vg, paint);
        nvgBatchedRender(vg);
    }


    gui_clip* guiClip = view.gui;
    track_t* track    = guiClip ? guiClip->m_track : NULL;
    if (track && track->audio) {
        clip_t* clip = guiClip->m_clip;

/*        ThreadLock lock                       = MainCtrl::getPlayThread()->lockThread();
        std::vector<note_t> heldRealtimeNotes = vsthost::getInstance()->getRealtimeNotes();//TODO: NOT THREADSAFE
        if (heldRealtimeNotes.size()) {
            int nRendered = 0;
            for (note_t& note: heldRealtimeNotes) {
                if (note.isRealtime()) {
                    continue;
                }
                tick_t pos = note.start() - clip->start() + clip->offsetStart;
                if (clip->isLoopEnabled()) {
                    if (pos > clip->loopStart) {
                        pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
                    }
                }
                //TODO: CULL
                renderNote(vg, this, &note, scale, -note.start() + pos);
                nRendered++;
            }
            if (nRendered) {
                NVGpaint paint{};
                paint.image      = -1;
                paint.innerColor = theme->getColor(GuiColor::COL_NOTE_REALTIME);
                paint.customPar  = 2;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }*/
        ThreadLock lock                = track->audio->midiMutex.lockThread();
        std::vector<note_t>& heldNotes = track->audio->m_heldNotes;
        if (heldNotes.size()) {
            int nRendered = 0;
            for (note_t& note: heldNotes) {
                if (note.isRealtime()) {
                    continue;
                }
                tick_t pos = note.start() - clip->start() + clip->offsetStart;
                if (clip->isLoopEnabled()) {
                    if (pos > clip->loopStart) {
                        pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
                    }
                }
                //TODO: CULL
                renderNote(vg, this, &note, scale, -note.start() + pos);
                nRendered++;
            }
            if (nRendered) {
                NVGpaint paint{};
                paint.image      = -1;
                paint.innerColor = theme->getColor(GuiColor::COL_NOTE_PLAYING);
                paint.customPar  = 2;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
        }

        if (track->audio->arp->isProcessingEnabled()) {
            auto& heldNotesArp = track->audio->getArpHeldNotes();//TODO: NOT THREADSAFE
            if (heldNotesArp.size()) {
                nvgBeginPath(vg);

                for (auto& note: heldNotesArp) {
                    tick_t pos = note.start() - clip->start() + clip->offsetStart;
                    if (clip->isLoopEnabled()) {
                        if (pos > clip->loopStart) {
                            pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
                        }
                    }
                    //TODO: CULL
                    renderNote(vg, this, &note, scale, -note.start() + pos);
                }
                NVGpaint paint{};
                paint.image      = -1;
                paint.innerColor = theme->getColor(GuiColor::COL_NOTE_ARP);
                paint.customPar  = 3;
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
            
        }

        float yoff = 0;
        for (int i = 0; i < 2; i++) {
            std::vector<marker_t> markers = track->audio->getArpMarkers(i);//TODO: NOT THREADSAFE
            if (markers.size()) {
                for (marker_t& m: markers) {
                    tick_t pos = m.time - clip->start() + clip->offsetStart;
                    if (clip->isLoopEnabled()) {
                        if (pos > clip->loopStart) {
                            pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
                        }
                    }
                    float nx = grid.tickToScreenD(pos);
                    if (nx < -4)
                        continue;
                    if (nx > w + 4)
                        continue;
                    nvgBeginPath(vg);
                    nvgMoveTo(vg, nx, m.yOffset * 24 + 0 + yoff);
                    nvgLineTo(vg, nx, m.yOffset * 24 + h + yoff);
                    nvgStrokeColor(vg, rgbToNvg(m.color));
                    nvgStrokeWidth(vg, 2.0f);
                    nvgStroke(vg);
                    if (m.desc[0]) {
                        String cstr = m.desc;
                        setFont(vg, G_FONT_SCALE(24), THEMECOL_TEXT, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                        float bounds[4];
                        float textX = nx + INSET_TRACK_CONTENT;
                        float textY = m.yOffset * 24 + 24 / 2.0 + INSET_TRACK_CONTENT + yoff;
                        nvgTextBounds(vg, textX, textY, cstr.c_str(), nullptr, bounds);
                        nvgBeginPath(vg);
                        nvgRect(vg, bounds[0], bounds[1], bounds[2] - bounds[0], bounds[3] - bounds[1]);
                        nvgFillColor(vg, rgbaToNvg(i == 0 ? 0xFF121212 : 0xFF444412));
                        nvgFill(vg);
                        nvgFillColor(vg, THEMECOL_TEXT);
                        nvgText(vg, textX, textY, cstr.c_str(), nullptr);
                    }
                    yoff += 3;
                }
            }
        }
    }

    if (scale >= 18) {
        int idx = 0;
        for (note_t& note: notes.m_list) {
            tick_t absPos = note.start();
            if (view.clip()) {
                absPos = note.start() + view.clip()->start() - view.clip()->offsetStart;
            }
            //TODO: CULL
            renderNoteName(vg, this, &note, idx++, scale, absPos);
        }
    }

    x = (float) grid.tickToScreenD(view.cursor.start);
    if (view.cursor.start == view.cursor.end) {
        if (x >= -2 && x < size.x + 2) {
            x += 0.5;
            NVGcolor cursorColor = getCursorColor();
            nvgBeginPath(vg);
            nvgMoveTo(vg, x, 1);
            nvgLineTo(vg, x, size.y - 1);
            nvgStrokeColor(vg, cursorColor);
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }
    } else {
        float x2 = (float) grid.tickToScreenD(view.cursor.end);
        if (x2 > -4.0f && x < size.x + 4.0f) {
            float xBegin = CLAMP_I(x, -4.0f, size.x + 3.0f);
            float xEnd   = CLAMP_I(x2, -3.0f, size.x + 4.0f);
            float width  = xEnd - xBegin;
            nvgBeginPath(vg);
            nvgRect(vg, xBegin, -2.0f, width, size.y + 2.0f);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_SELECTION_BACKGROUND));
            nvgFill(vg);
        }
    }

    if (dragMode == drag_frame) {
        renderFrame(vg, dragBegin, dragTo);
    }
}

void gui_clipcontent::handleDraggedBegin(MouseEvent& evt) {
    dragMode     = drag_none;
    clip_t* clip = view.clip();
    if (!clip) {
        return;
    }
    clip_notes_t& notes    = clip->notes;
    ivec2 local            = evt.relMousepos;
    int32_t pitch          = toNoteF(local.y);
    int32_t velClicked     = screenToVel(local.y, size.y);
    const tick_t tickExact = grid.screenToTickSnap(local.x, SNAP_OFF);
    note_t* contextNote    = nullptr;
    if (isVelocity) {
        int32_t velDist = VEL_SELECT_DISTANCE * 127 / size.y;
        contextNote     = getMinDistNoteVel(notes, tickExact, grid.pixelsToTicks(VEL_SELECT_DISTANCE), velClicked, velDist);
    } else {
        contextNote = notes.get(tickExact, pitch);
    }

    tick_t tickGridNearest = grid.screenToTickSnap(local.x, SNAP_ON);
    tick_t tickGridLeast   = grid.prev(tickExact);
    if (evt.type == M_EVT_DOUBLECLICK) {
        ThreadLock lock            = MainCtrl::getPlayThread()->lockThread();
        clip_cursor_t cursorBefore = view.cursor;
        notes.clearSelection();
        clip_notes_t notesBefore = notes;
        if (contextNote) {
            contextNote = notes.get(tickExact, pitch);
        }
        String desc = "???";
        if (contextNote != NULL) {
            if (!isVelocity) {
                view.cursor.start = view.cursor.end = contextNote->start();
                notes.remove(*contextNote);
                contextNote = NULL;
                desc        = "Delete Note";
            } else {
                contextNote->toggleFlag(NoteFlags::ENABLED);
            }
        } else {
            if (!isVelocity) {
                note_t note;
                note.pitch = pitch;
                note.time  = tickGridLeast;
                note.len   = grid.getTickLength();
                notes.paste(note);
                contextNote = notes.get(tickGridLeast, pitch);
                notes.selection.insert(contextNote);
                view.copySelectedNoteList();
                MainCtrl::get()->setStatusText(StringFormat("%d %d %d", note.pitch, note.time, note.len));
                desc = "Add Note";
                setSelectionFrame(getMinMaxTime(view.draggedSelection));
            }
        }
        DawInstance::get()->pushHist(new action_modify_notes(desc, view, notesBefore, cursorBefore));
        clip->setDirty();
        view.updateNotePitches(false);
    } else {

        bool inSelection = false;
        if (contextNote != NULL) {
            inSelection = stl_contains(notes.selection, contextNote);
            if (!inSelection) {
                notes.clearSelection();
                if (isVelocity) {
                    int32_t velDist = VEL_SELECT_DISTANCE * 127 / size.y;
                    contextNote     = getMinDistNoteVel(notes, tickExact, grid.pixelsToTicks(VEL_SELECT_DISTANCE), velClicked, velDist);
                } else {
                    contextNote = notes.get(tickExact, pitch);
                }
                if (contextNote) {
                    beginDragNote = *contextNote;
                    notes.selection.insert(contextNote);
                    view.copySelectedNoteList();
                    inSelection = true;
                }
            }
            if (contextNote != NULL) {
                beginDragNote = *contextNote;
            }
        }
        if (inSelection) {
            if (isVelocity) {
                dragMode = drag_velocity;
            } else {
                auto distL = local.x - grid.tickToScreenD(contextNote->start());
                auto distR = grid.tickToScreenD(contextNote->end()) - local.x;
                if (distL < DRAG_RANGE) {
                    dragMode = drag_note_left;
                }
                if (distR < DRAG_RANGE && (distL >= DRAG_RANGE || distR < distL)) {
                    dragMode = drag_note_right;
                }
                if (dragMode == drag_none) {
                    if (isCtrl(evt.kbmods)) {
                        parentCtrl->cursorIcon = CURSOR_DUPLICATE;
                        dragMode               = drag_notes_copy;
                    } else {
                        dragMode = drag_notes_move;
                    }
                }
            }
            notes.removeDuplicates();
            view.copySelectedNoteList();

            notes.selection.clear();
            setSelectionFrame(getMinMaxTime(view.draggedSelection));
            dragStartCursor = view.cursor;
        } else {
            if (notes.selection.empty())
                notes.removeDuplicates();
            if (isShift(evt.kbmods)) {
                selectionStart = notes.selection;
                if (math::abs(view.cursor.start - tickGridNearest) < math::abs(view.cursor.end - tickGridNearest)) {
                    view.cursor.start = tickGridNearest;
                } else {
                    view.cursor.end = tickGridNearest;
                }
            } else {
                selectionStart.clear();
                notes.clearSelection();
                view.cursor.start = view.cursor.end = tickGridNearest;
                view.copySelectedNoteList();
            }

            dragMode = drag_frame;
        }
    }
    if (dragMode != drag_none) {
        dragBegin = local;
        dragTo    = local;
    }
    setStatusText();
    setGlobalSelectionFromClipSelection();
}
void gui_clipcontent::setGlobalSelectionFromClipSelection() {
    clip_t* clip = view.clip();
    if (!clip) {
        return;
    }
    DAW::Cursor& cursor = dawCtrl->getCursor();
    cursor.cursorPos    = view.cursor.start + clip->start() - clip->offsetStart;
    cursor.selRange     = view.cursor.end - view.cursor.start;
}
void gui_clipcontent::setStatusText() {
    clip_notes_t& notes = view.clip()->notes;
    String selStatus    = StringFormat("%zu notes selected", notes.selection.size());
    if (!view.draggedSelection.empty()) {
        auto pair = getMinMaxSemitones(view.draggedSelection);
        if (pair.first && pair.second) {
            selStatus += " - ";
            selStatus += StringFormat("pitch %d to %d", pair.first->pitch, pair.second->pitch);
            selStatus += " - ";
            auto pair2 = getMinMaxTime(view.draggedSelection);
            selStatus += StringFormat("time %d to %d", pair2.first->start(), pair2.second->end());
        }
    }
    MainCtrl::get()->setStatusText(selStatus);
}

void gui_clipcontent::handleDraggedMove(MouseEvent& evt) {
    clip_t* clip = view.clip();
    if (!clip)
        return;
    clip_notes_t& notes = clip->notes;
    if (dragMode == drag_none)
        return;
    dragTo = evt.relMousepos;
    if (dragMode == drag_frame) {
        *evt.dragDistance     = ivec2(0);

        float xStart     = math::min(dragBegin.x, dragTo.x);
        float xEnd       = math::max(dragBegin.x, dragTo.x);
        float yStart     = math::min(dragBegin.y, dragTo.y);
        float yEnd       = math::max(dragBegin.y, dragTo.y);
        tick_t tickStart = grid.screenToTickSnap(xStart, SNAP_OFF);
        tick_t tickEnd   = grid.screenToTickSnap(xEnd, SNAP_OFF);
        tick_t tickOver  = grid.screenToTickSnap(evt.relMousepos.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);

        clip_cursor_t& cursor = view.cursor;
        if (isShift(evt.kbmods)) {
            tick_t gridSize = grid.getTickLength();
            if (math::abs(cursor.start - tickOver) < math::abs(cursor.end - tickOver)) {
                if (tickOver < cursor.end - gridSize) {
                    cursor.start = tickOver;
                }
            } else {
                if (tickOver > cursor.start + gridSize) {
                    cursor.end = tickOver;
                }
            }
        } else {
            cursor.start = grid.screenToTickSnap(xStart, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
            cursor.end   = grid.screenToTickSnap(xEnd, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
            setSelectionFrame(getMinMaxTime(view.draggedSelection));
        }
        notes.selection = selectionStart;
        if (!isVelocity) {

            int32_t pitchLow  = toNoteF(yEnd);
            int32_t pitchHigh = toNoteF(yStart);
            std::vector<note_t*> inRangeList;
            if (notes.getInRange(tickStart, tickEnd, pitchLow, pitchHigh, inRangeList)) {
                std::set<note_t*>& selection = notes.selection;
                for (note_t* inSelRange: inRangeList) {
                    auto result = selection.insert(inSelRange);
                    if (!result.second) {
                        selection.erase(result.first);
                    }
                }
            }
        } else {
            int32_t velLow  = screenToVel(yEnd, size.y);
            int32_t velHigh = screenToVel(yStart, size.y);
            std::vector<note_t*> inRangeList;
            if (notes.getStartsInRangeV(tickStart, tickEnd, velLow, velHigh, grid.pixelsToTicks(VEL_SELECT_DISTANCE), inRangeList)) {
                std::set<note_t*>& selection = notes.selection;
                for (note_t* inSelRange: inRangeList) {
                    auto result = selection.insert(inSelRange);
                    if (!result.second) {
                        selection.erase(result.first);
                    }
                }
            }
        }
        if (!isShift(evt.kbmods)) {
            if (!notes.selection.empty()) {
                auto pair = getMinMaxTime(notes.selection);
                expandSelectionFrame(pair);
            }
        }
        view.copySelectedNoteList();

        setStatusText();
    } else if (dragMode == drag_velocity) {
        *evt.dragDistance = ivec2(0);
        //    tick_t tickOver = grid.screenToTickSnap(evt.relMousepos.x, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
        //    clip_cursor_t& cursor = view.cursor;
        ThreadLock lock       = MainCtrl::getPlayThread()->lockThread();
        int32_t velOffset     = (dragBegin.y - dragTo.y) * 127 / size.y;
        view.draggedSelection = view.draggedSelectionBegin;
        {
            auto it          = view.draggedSelection.begin();
            const auto itEnd = view.draggedSelection.end();
            while (it != itEnd) {
                note_t& note  = *it;
                note.velocity = math::min(127, math::max(0, note.velocity + velOffset));
                it++;
            }
        }
        mergeDraggedNotes(dragMode);
        setSelectionFrame(getMinMaxTime(view.draggedSelection));

    } else if (dragMode >= drag_notes_move) {
        ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
        int modeMove    = SNAP_LEAST;
        if (isAlt(evt.kbmods)) {
            modeMove = SNAP_OFF;
        }


        tick_t gridSize       = grid.getTickLength();
        int32_t pitchStart    = toNoteFNoFolding(dragBegin.y);
        int32_t pitchEnd      = toNoteFNoFolding(dragTo.y);
        tick_t pitchOffset    = pitchEnd - pitchStart;
        tick_t tickStartExact = grid.screenToTick(dragBegin.x);
        tick_t tickEndExact   = grid.screenToTick(dragTo.x);
        tick_t timeOffsetEx   = tickEndExact - tickStartExact;

        tick_t timeOffset = 0;
        const note_t noteDrag = this->beginDragNote;
        if (modeMove == SNAP_LEAST) {
            tick_t handlePos = dragMode == drag_note_right ? noteDrag.end() : noteDrag.start();
            if (math::abs(timeOffsetEx) > gridSize / 4) {
                tick_t next = grid.next(handlePos + timeOffsetEx) - handlePos;
                tick_t prev = grid.prev(handlePos + timeOffsetEx) - handlePos;
                if (prev < 0 && timeOffsetEx > 0) {
                    prev = next;
                }
                if (next > 0 && timeOffsetEx < 0) {
                    next = prev;
                }
                if (math::abs(next) > math::abs(prev)) {
                    timeOffset = prev;
                } else {
                    timeOffset = next;
                }
            }
        } else {
            timeOffset = timeOffsetEx;
        }
        view.draggedSelection = view.draggedSelectionBegin;
        {
            auto it          = view.draggedSelection.begin();
            const auto itEnd = view.draggedSelection.end();
            while (it != itEnd) {
                note_t& note = *it;
                if (dragMode == drag_note_left) {
                    note_t* before = getFirstBefore(notes.m_list, note.pitch, note.time);
                    note.time      = math::min(note.end() - 1, note.start() + timeOffset);
                    note.len       = math::max(math::max(1, gridSize/4), note.len - timeOffset);
                    if (before) {
                        if (note.start() < before->end()) {
                            note.cutLeft(before->end());
                        }
                    }
                } else if (dragMode == drag_note_right) {
                    note_t* after = getFirstAfter(notes.m_list, note.pitch, note.time);
                    note.len      = math::max(math::max(1, gridSize/4), note.len + timeOffset);
                    if (after) {
                        if (note.end() > after->start()) {
                            note.cutRight(after->start());
                        }
                    }
                } else {
                    note.time += timeOffset;
                    if (layoutRoll.fold) {
                        note.pitch = view.nextFoldNote(note.pitch, pitchOffset);
                    } else {
                        note.pitch += pitchOffset;
                    }
                }

                it++;
            }
        }
        {
            std::vector<note_t> notesDraggedCopy = view.draggedSelection;
            std::vector<note_t> notesDraggedNoDuplicates = view.draggedSelection;

            auto it    = notesDraggedCopy.begin();
            auto itEnd = notesDraggedCopy.end();
            while (it != itEnd) {
                note_t& noteTest = *it++;
                if (cutIntersecting(notesDraggedNoDuplicates, noteTest, false) > 0) {
                    notesDraggedCopy = notesDraggedNoDuplicates;
                    it    = notesDraggedCopy.begin();
                    itEnd = notesDraggedCopy.end();
                }
            }
            view.draggedSelection = notesDraggedNoDuplicates;
        }
        mergeDraggedNotes(dragMode);
        setSelectionFrame(getMinMaxTime(view.draggedSelection));
    }
    setGlobalSelectionFromClipSelection();
}
bool gui_clipcontent::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {

        ivec2 local = this->toContainerSpace(mpos);
        for (guibase* gui: guis) {
            if (gui->mouseHitTest(local, evt)) {
                return true;
            }
        }
        if (!isVelocity && view.clip() && evt.type <= MouseHitType::MOUSE_RIGHT) {
            auto pitch     = toNoteF(local.y);
            auto tickExact = grid.screenToTickSnap(local.x, SNAP_OFF);
            const auto* contextNote = view.clip()->notes.get(tickExact, math::floorfS32(pitch));
            if (contextNote) {
                auto distL = local.x - grid.tickToScreenD(contextNote->start());
                auto distR = grid.tickToScreenD(contextNote->end()) - local.x;
                if (distL < DRAG_RANGE) {
                    evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
                }
                if (distR < DRAG_RANGE && (distL >= DRAG_RANGE || distR < distL)) {
                    evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
                }
            }
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}

void gui_clipcontent::mergeDraggedNotes(dragmode mergeMode) {
    clip_t* clip        = view.clip();
    clip_notes_t& notes = clip->notes;
    notes               = view.dragStartNotes;
    notes.selection.clear();
    if (mergeMode != dragmode::drag_notes_copy) {
        notes.removeAllKeepDuplicates(view.draggedSelectionBegin);
    }
    for (note_t& note: view.draggedSelection) {
        notes.paste(note, true);
    }
    notes.selectLastN(view.draggedSelection.size());
    clip->setDirty();
    view.updateNotePitches(false);
}
void gui_clipcontent::expandSelectionFrame(std::pair<note_t*, note_t*> minMax) {
    if (minMax.first && minMax.second) {
        auto& cursor = view.cursor;
        cursor.start = math::min(cursor.start, minMax.first->time);
        cursor.end   = math::max(cursor.end, (minMax.second->time + minMax.second->len));
    }
}
void gui_clipcontent::setSelectionFrame(std::pair<note_t*, note_t*> minMax) {
    if (minMax.first && minMax.second) {
        auto& cursor = view.cursor;
        cursor.start = minMax.first->time;
        cursor.end   = minMax.second->time + minMax.second->len;
    }
}
void gui_clipcontent::handleDraggedRelease(MouseEvent& evt) {
    if (dragMode == drag_frame) {
        handleDraggedMove(evt);
        dragMode = drag_none;
        return;
    }
    clip_t* clip = view.clip();
    if (clip) {
        if (dragMode >= drag_notes_move) {
            ThreadLock lock     = MainCtrl::getPlayThread()->lockThread();
            clip_notes_t& notes = clip->notes;
            mergeDraggedNotes(dragMode);
            setSelectionFrame(getMinMaxTime(notes.selection));
            String action;
            if (dragMode == drag_velocity) {
                action = "Modify note velocities";
            } else if (dragMode >= drag_note_left) {
                action = "Modify note lengths";
            } else {
                action = "Move notes";
            }
            DawInstance::get()->pushHist(new action_modify_notes(action, view, view.dragStartNotes, dragStartCursor));
            view.copySelectedNoteList();
            clip->setDirty();
            view.updateNotePitches(false);
        }
    }
    dragMode = drag_none;
    setGlobalSelectionFromClipSelection();
}

bool gui_clipcontent::handleKeyInput(KeyEvent& kevt) {
    clip_t* clip = view.clip();
    if (!clip) {
        return false;
    }
    ThreadLock lock     = MainCtrl::getPlayThread()->lockThread();
    clip_notes_t& notes = clip->notes;
    if (kevt.type != STATE_REPEAT && isCtrlKey(kevt.keyCode)) {
        if ((dragMode == drag_notes_move || dragMode == drag_notes_copy)) {
            if ((dragMode == drag_notes_copy) != (kevt.type == STATE_PRESS)) {
                if (dragMode == drag_notes_move) {
                    dragMode               = drag_notes_copy;
                    parentCtrl->cursorIcon = CURSOR_DUPLICATE;
                } else {
                    dragMode               = drag_notes_move;
                    parentCtrl->cursorIcon = CURSOR_DEFAULT;
                }
                mergeDraggedNotes(dragMode);
            }
            return false;
        }
    }
    if (dragMode) {
        return false;
    }
    if (kevt.type != K_RELEASE) {
        clip_cursor_t& cursor          = view.cursor;
        const clip_notes_t notesBefore = notes; // copy
        clip_cursor_t cursorBefore     = cursor;// copy
        bool handled                   = false;
        bool edit                      = false;
        String desc                    = "???";
        if (kevt.type == K_PRESS) {
            if (isKC(KC_SELECTALL, kevt)) {
                notes.clearSelection();
                notes.updateBounds();
                notes.selectIdxRange(0, notes.m_list.size());
                view.copySelectedNoteList();
                setSelectionFrame(getMinMaxTime(notes.selection));
                handled = true;
            }
            if (isKC(KC_DELETE, kevt) && !notes.selection.empty()) {
                notes.deleteSelectedNotes(notes);
                handled = true;
                edit    = true;
                desc    = "Delete notes";
            }
            if (isKC(KC_MUTE, kevt) && !notes.selection.empty()) {
                //        notes.muteToggleSelectedNotes(notes);
                muteNotesToggle(view.draggedSelection);
                mergeDraggedNotes(dragmode::drag_notes_move);
                notes.updateBounds();
                // setSelectionFrame(getMinMaxTime(notes.selection));
                handled = true;
                edit    = true;
                desc    = "Mute notes";
            } else if (isKC(KC_CUT, kevt) && !notes.selection.empty()) {
                view.clipboardCursorRange = cursor.end - cursor.start;
                view.clipboard.setTo(notes.selection, -cursor.start);
                notes.deleteSelectedNotes(notes);
                handled = true;
                edit    = true;
                desc    = "Cut notes";
            } else if (isKC(KC_COPY, kevt) && !notes.selection.empty()) {
                view.clipboardCursorRange = cursor.end - cursor.start;
                view.clipboard.setTo(notes.selection, -cursor.start);
                handled = true;
                desc    = "Copy notes";// never appears in list
            } else if (isKC(KC_DUPLICATE, kevt) && !notes.selection.empty()) {
                clip_notes_t tmpClipboard;
                for (note_t* selPtr: notes.selection) {
                    dbgassert(notes.has(selPtr));
                }
                tmpClipboard.setTo(notes.selection, -cursor.start);
                tick_t cursorRange = cursor.end - cursor.start;
                cursor.start += cursorRange;
                cursor.end += cursorRange;
                notes.clearSelection();
                view.copySelectedNoteList();
                view.draggedSelection.clear();
                for (note_t note: tmpClipboard.m_list) {//not using reference here, copy while iterating
                    note.time += cursor.start;
                    view.draggedSelection.push_back(note);
                }
                mergeDraggedNotes(dragmode::drag_notes_copy);
                for (note_t* selPtr: notes.selection) {
                    dbgassert(notes.has(selPtr));
                }
                auto pair = getMinMaxTime(notes.selection);
                if (pair.second)
                    grid.makeTickVisible(pair.second->end());
                handled = true;
                edit    = true;
                desc    = "Duplicate notes";
            } else if (isKC(KC_PASTE, kevt) && !view.clipboard.empty()) {
                notes.clearSelection();
                view.copySelectedNoteList();
                view.draggedSelection.clear();
                for (note_t note: view.clipboard.m_list) {//not using reference here, copy while iterating
                    note.time += cursor.start;
                    view.draggedSelection.push_back(note);
                }
                mergeDraggedNotes(dragmode::drag_notes_move);
                view.cursor.end = cursor.start + view.clipboardCursorRange;
                auto pair = getMinMaxTime(notes.selection);
                if (pair.second)
                    grid.makeTickVisible(pair.second->end());
                handled = true;
                edit    = true;
                desc    = "Paste notes";
            } else if (isKC(KC_QUANTIZE, kevt) && !notes.selection.empty()) {
                auto& settings = project_controller_t::get()->getQuantizeSettings();
                if (settings.quantizeStart > 0 || settings.quantizeEnd > 0) {
                    log_lf(Log::L_DEBUG, "quantize to %d %d\n", settings.quantizeStart, settings.quantizeEnd);
                    /* Quantize notes to grid 
                     * 1. cut notes from clip
                     * 2. quantize notes in isolation
                     * 3. paste notes back to clip, cutting intersections
                     */
                    clip_notes_t tmpClipboard;
                    tmpClipboard.setTo(notes.selection, 0);
                    notes.deleteSelectedNotes(notes);
                    notes.clearSelection();
                    view.copySelectedNoteList();
                    view.draggedSelection.clear();
                    if (settings.quantizeStart > 0) {
                        quantizeNoteStartTime(tmpClipboard.m_list, settings.quantizeStart);
                    }
                    if (settings.quantizeEnd > 0) {
                        quantizeNoteEndTime(tmpClipboard.m_list, settings.quantizeEnd);
                    }
                    bool bRemovedNotes = cutSelfIntersecting(tmpClipboard.m_list);
                    if (bRemovedNotes) {
                        log_lf(Log::L_DEBUG, "removed some intersecting notes\n");
                    }
                    for (note_t note: tmpClipboard.m_list) {//not using reference here, copy while iterating
                        view.draggedSelection.push_back(note);
                    }
                    mergeDraggedNotes(dragmode::drag_notes_copy);
                    for (note_t* selPtr: notes.selection) {
                        dbgassert(notes.has(selPtr));
                    }
                    auto pair = getMinMaxTime(notes.selection);
                    if (pair.second)
                        grid.makeTickVisible(pair.second->end());
                    expandSelectionFrame(pair);
                    edit = true;
                    handled = true;
                    desc = "Quanitize notes";
                }
            }
        } else {
        }
        if (isArrowKey(kevt.keyCode)) {
            ivec2 dir;
            arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
            if (dir.y && !notes.selection.empty()) {
                if ((kevt.mods & KB_MOD_SHIFT)) {
                    dir *= 12;
                }
                changePitch(view.draggedSelection, dir.y,
                            layoutRoll.fold, layoutRoll.fold ? view.notePitches : std::vector<int32_t>{});
                mergeDraggedNotes(dragmode::drag_notes_move);
                notes.updateBounds();
                setSelectionFrame(getMinMaxTime(notes.selection));
                auto pair = getMinMaxSemitones(view.draggedSelection);
                if (dir.y < 0) {
                    if (pair.first) {
                        makeNoteVisible(pair.first->pitch);
                    }
                } else if (dir.y > 0) {
                    if (pair.second) {
                        makeNoteVisible(pair.second->pitch);
                    }
                }
                edit = true;

            } else if (dir.x) {
                tick_t timeOffset = dir.x;
                tick_t minLen     = grid.pixelsToTicks(2);
                if (!isAlt(kevt.mods)) {
                    minLen = grid.getTickLength();
                }
                timeOffset *= minLen;
                cursor.start += timeOffset;
                cursor.end += timeOffset;
                if (!notes.selection.empty()) {
                    if ((kevt.mods & KB_MOD_SHIFT)) {
                        offsetEndTime(view.draggedSelection, timeOffset, minLen);
                    } else {
                        offsetStartTime(view.draggedSelection, timeOffset);
                    }
                    mergeDraggedNotes(dragmode::drag_notes_move);
                    notes.updateBounds();
                    setSelectionFrame(getMinMaxTime(notes.selection));
                    auto pair = getMinMaxTime(notes.selection);
                    if (dir.x < 0) {
                        if (pair.first) {
                            grid.makeTickVisible(pair.first->start());
                        }
                    } else if (dir.x > 0) {
                        if (pair.second) {
                            grid.makeTickVisible(pair.second->end());
                        }
                    }
                    edit = true;
                } else {
                    grid.makeTickVisible(cursor.start);
                }
            }
            handled = true;
            desc    = "Move notes";
        }
        if (edit) {
            notes.updateBounds();
            DawInstance::get()->pushHist(new action_modify_notes(desc, view, notesBefore, cursorBefore));
            clip->setDirty();
            view.updateNotePitches(false);
        }
        return handled;
    }
    return false;
}

guictr_clipeditorview::guictr_clipeditorview(clip_view& _view, guictr_noteeditor& _noteeditor)
    : guictr_base(),
      view(_view),
      cache(new midi_clip_render_cache_t{}),
      noteeditor(_noteeditor),
      grid(_noteeditor.grid)
{
}
guictr_clipeditorview::~guictr_clipeditorview() {
    delete cache;
}

void guictr_clipeditorview::resetCache() {
    cache->reset();
}

void guictr_clipeditorview::prerender(NVGcontext* vg) {
    clip_view& view  = dawCtrl->getClipView();
    clip_t* const cl = view.clip();
    if (!cl) {
        cache->reset();
        return;
    }
    noteview_render_t& notesView = cl->getNoteViewRender();

    // ivec2 cp = this->getPosContent();
    ivec2 sizeContents  = this->getSizeContent();
    ivec2 clipPosScreen = toScreenSpace(ivec2(0, 0));

    tick_t clipLen = cl->getLen();
    float numBars  = clipLen / (float) TICKS_BAR;
    float barSize  = sizeContents.x / (float) numBars;

    bool cacheValid = cache->revision == notesView.reqRevision;
    cacheValid &= cache->valid;
    cacheValid &= cache->pos == clipPosScreen;
    cacheValid &= cache->size == sizeContents;
    if (!cacheValid) {

	    nvgReset(vg);
        nvgScale(vg, parentCtrl->m_scale, parentCtrl->m_scale);
        nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);
        nvgCachePath(vg, 1);
        int64_t notesRendered = 0;

        cache->reset();

        NVGcolor rgbNote        = theme->getColor(GuiColor::COL_CLIP_NOTE);
        NVGcolor rgbNoteOverlap = theme->getColor(GuiColor::COL_CLIP_NOTE_OVERLAP);
        NVGcolor rgbNoteMuted   = theme->getColor(GuiColor::COL_CLIP_NOTE_MUTED);

        int noteRenderMode = theme->get(GuiConstant::CONST_NOTE_RENDER_MODE);
        nvgSave(vg);
        nvgTranslate(vg, clipPosScreen.x, clipPosScreen.y);
        nvgSave(vg);
        // nvgTranslate(vg, 0, 0);

        if (sizeContents.x > 0 && sizeContents.y > 0) {
            clip_notes_t& notes = notesView;
            if (!notes.empty()) {
                note_t minN      = notesView.minNote;
                note_t maxN      = notesView.maxNote;
                int32_t numNotes = math::max((int32_t) 8, maxN.pitch - minN.pitch);
                float scale      = sizeContents.y / (float) numNotes;
                std::vector<const note_t*> notesClipped;
                std::vector<const note_t*> notesMuted;
                int begin = 0;
                for (const note_t& note : notes.m_list) {
                    tick_t noteTime = note.time;
                    if (noteTime >= clipLen) {
                        notesClipped.push_back(&note);
                        continue;
                    }
                    if (noteTime < 0) {
                        notesClipped.push_back(&note);
                        continue;
                    }
                    if (!note.isEnabled()) {
                        notesMuted.push_back(&note);
                        continue;
                    }
                    if (!begin) {
                        if (noteRenderMode == 0) {
                            nvgBeginPath(vg);
                        }
                        begin++;
                    }
                    float objPosNote = noteTime / (float) TICKS_BAR;
                    float objLenNote = note.len / (float) TICKS_BAR;
                    float ny     = noteToScreen(note.pitch - minN.pitch, scale, 0, sizeContents.y);
                    float nx     = math::max(0.0f, objPosNote * barSize);
                    float nw     = math::min(objLenNote * barSize, sizeContents.x - nx);
                    float nh     = scale;
                    float insetx = calcInset(1, nw);
                    float insety = calcInset(1, nh);
                    if (noteRenderMode == 0) {
                        nvgRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                    } else {
                        nvgBatchedRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                    }

                    notesRendered++;
                }
                if (begin) {
                    if (noteRenderMode == 0) {
                        nvgFillColor(vg, rgbNote);
                        nvgSetShapeExtents(vg, 0, 0, sizeContents.x, sizeContents.y);
                        nvgFill(vg);
                    } else {
                        NVGpaint paint;
                        memset(&paint, 0, sizeof(paint));
                        paint.image      = -1;
                        paint.innerColor = rgbNote;
                        paint.outerColor = rgbNote;
                        paint.customPar  = 1234;
                        nvgFillPaint(vg, paint);
                        nvgBatchedRender(vg);
                    }
                    cache->SaveFill(vg, 0);
                }

                for (int j = 0; j < 2; j++) {
                    auto& list = j == 0 ? notesClipped : notesMuted;
                    if (!list.empty()) {
                        if (noteRenderMode == 0) {
                            nvgBeginPath(vg);
                        }
                        for (const note_t* noteClipped : list) {
                            const note_t& note = *noteClipped;
                            tick_t noteTime    = note.time;

                            float objPosNote = noteTime / (float) TICKS_BAR;
                            float objLenNote = note.len / (float) TICKS_BAR;
                            float ny         = noteToScreen(note.pitch - minN.pitch, scale, 0, sizeContents.y);
                            float nx         = objPosNote * barSize;
                            float nw         = objLenNote * barSize;
                            float nh         = scale;
                            float insetx     = calcInset(1, nw);
                            float insety     = calcInset(1, nh);
                            if (noteRenderMode == 0) {
                                nvgRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                            } else {
                                nvgBatchedRect(vg, nx + insetx, ny + insety, nw - insetx * 2, nh - insety * 2);
                            }
                            notesRendered++;
                        }

                        if (noteRenderMode == 0) {
                            nvgFillColor(vg, j == 0 ? rgbNoteOverlap : rgbNoteMuted);
                            nvgSetShapeExtents(vg, 0, 0, sizeContents.x, sizeContents.y);
                            nvgFill(vg);
                        } else {
                            NVGpaint paint;
                            memset(&paint, 0, sizeof(paint));
                            paint.image      = -1;
                            paint.innerColor = j == 0 ? rgbNoteOverlap : rgbNoteMuted;
                            paint.outerColor = j == 0 ? rgbNoteOverlap : rgbNoteMuted;
                            paint.customPar  = 1234;
                            nvgFillPaint(vg, paint);
                            nvgBatchedRender(vg);
                        }

                        if (j == 0) {
                            cache->SaveFill(vg, 1);
                        } else {
                            cache->SaveFill(vg, 2);
                        }
                    }
                }
            }
        }
        nvgRestore(vg);
        nvgCachePath(vg, 0);

        cache->valid          = true;
        cache->pos            = clipPosScreen;
        cache->size           = sizeContents;
        cache->notesRendered  = notesRendered;
        cache->revision       = notesView.reqRevision;
    }
    
    for (guibase* gui : guis) {
        gui->prerender(vg);
    }
}

float guictr_clipeditorview::getScaleX() {
    float scaleX = 1.0f;
    auto* clip = view.clip();
    if (clip) {
        auto barBeginEditor = grid.toObjSpace(0.0);
        auto barEndEditor = grid.toObjSpace(noteeditor.content.size.x);
        auto barLenClip = clip->getLen() / static_cast<double>(TICKS_BAR);
        auto barLenEditor = barEndEditor - barBeginEditor;
        scaleX  = math::max(static_cast<float>(barLenEditor/barLenClip), 0.0f);
    }
    return scaleX;
}

float guictr_clipeditorview::getScreenSpaceScaleX() {
    auto cs = getSizeContent();
    auto csEditor = noteeditor.timeline.size;
    if (cs.x <= 0)
        return 1.0f;
    return csEditor.x / static_cast<float>(cs.x);
}
void guictr_clipeditorview::getFrameBounds(vec2& posFrame, vec2& sizeFrame) {
    float scaleX = getScaleX();
    float scaleXSS = getScreenSpaceScaleX();
    ivec2 posContents = this->getPosContent();
    ivec2 sizeContents = this->getSizeContent();
    float offset = static_cast<float>(grid.getOffset()) * scaleX/scaleXSS;
    auto rightBottom = vec2(posContents + sizeContents);
    posFrame = vec2(math::min<float>(posContents.x + offset, rightBottom.x), posContents.y);
    sizeFrame = vec2(math::min<float>(sizeContents.x * scaleX, rightBottom.x - posFrame.x), sizeContents.y);
}

void guictr_clipeditorview::render(NVGcontext* vg) {
    ivec2 posContents = this->getPosContent();
    ivec2 sizeContents = this->getSizeContent();

    bool visible = dawCtrl->isClipEditorVisible();
    bool focused = visible && noteeditor.focused();
    if (visible) {
        int topOffset = CTR_SPACING / 2 + 1;
        drawBackground(vg, theme, posContents + ivec2(0, -topOffset), sizeContents+ivec2(0, topOffset), margin, focused, false);
    }
    drawInsetBackground(vg, theme, posContents, sizeContents);

    clip_t* const cl = view.clip();
    if (cl && cache->valid) {
        NVGcolor color = rgbToNvg(cl->rgb);
        if (!cl->enabled) {
            color = rgbToNvg(0x333333);
        }
        dbgassert(cache->valid);
        if (cache->valid && std::any_of(cache->arr.cbegin(), cache->arr.cend(), [](const auto* ptr) { return !!ptr; })) {

            int64_t notesRendered = 0;

            nvgSave(vg);
            nvgTranslate(vg, posContents.x, posContents.y);
            nvgSave(vg);
            if (cache->isCacheValid(0)) {
                nvgFillFromCache(vg, cache->arr[0]);
            }
            if (cache->isCacheValid(1)) {
                nvgFillFromCache(vg, cache->arr[1]);
            }
            if (cache->isCacheValid(2)) {
                nvgFillFromCache(vg, cache->arr[2]);
            }
            notesRendered += cache->notesRendered;
            nvgRestore(vg);
            if (cl->isLoopEnabled()) {
                if (cache->isCacheValid(3)) {
                    nvgFillFromCache(vg, cache->arr[3]);
                }
            }
            nvgRestore(vg);
            daw_tls::getTls().runtime->renderStats.notesRendered += notesRendered;
        }
        daw_tls::getTls().runtime->renderStats.clipsRendered++;
    }
    auto* clip = view.clip();
    if (clip) {
        vec2 posFrame, sizeFrame;
        getFrameBounds(posFrame, sizeFrame);
        if (sizeFrame.x > 0 && sizeFrame.y > 0) {
            nvgBeginPath(vg);
            nvgRect(vg, posFrame.x, posFrame.y, sizeFrame.x, sizeFrame.y);
            nvgStrokeWidth(vg, 3);
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLUGIN_VIEW_FRAME));
            nvgStroke(vg);
        }
    }
}

float guictr_cliphandles::clipStartScrX() {
    return (float) (grid.tickToScreenD(view.clip()->offsetStart));
}

float guictr_cliphandles::clipEndScrX() {
    return (float) (grid.tickToScreenD(view.clip()->offsetStart + view.clip()->getLen()));
}

guictr_base* makeGuiClipEditor() {
    return new guictr_clipeditor(MainCtrl::get()->getClipView());
}
