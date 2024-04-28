#include "clipeditor.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "note.h"
#include "seq_time.h"
#include "cursor.h"
#include "keyboard.h"
#include "grid.h"
#include "host/midihost/midi_host.h"

#include "gui/contextmenu/contextmenu_daw.h"

gui_pianoroll::gui_pianoroll(clip_view_t& _view, layout_pianoroll_t& _layout)
    : guibase(),
      piano_scale(_layout, _view, size.y),
      view(_view) {
    keysX     = 0;
    widthKeys = 0;
}

int32_t toVel(vec2 note) {
    return math::clamp<int32_t>(math::roundfS32(note.x * 127.0f / 1024.0f), 0, 127);
}

void gui_pianoroll::handleDraggedBegin(MouseEvent& evt) {
    dragMode = dragmode::drag_none;
    if (evt.guiDragged == this) {
        ivec2 local = evt.relMousepos;
        if (local.x < keysX) {
            if (evt.type == M_EVT_DOUBLECLICK) {
                parent->handleDraggedBegin(evt);
                return;
            }
            dragMode = dragmode::drag_move_resize;
            parentCtrl->captureMouse(this);
            startDrag       = evt.relMousepos;
            dragDirection   = -1;
            dragPosObjSpace = toNoteF(evt.relMousepos.y);
        } else {
            vec2 note = getNoteFromPos(local);
            if (note.y >= 0 && note.y < MAX_OCTAVES * 12) {
                dragMode = dragmode::drag_piano_key;
                handleDraggedMove(evt);
            }
        }
    }
}

void gui_pianoroll::handleDraggedMove(MouseEvent& evt) {
    if (dragMode == dragmode::drag_move_resize) {
        dbgassert(evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE);
        if (evt.guiDragged == this && evt.type == M_EVT_CAPTURED_MOVE) {
            bool lockGesture = true;
            bool isMove      = true;
            if (lockGesture) {
                if (dragDirection < 0) {
                    float initialx = (float) (math::abs(evt.mousepos.x - evt.dragStart.x));
                    float initialy = (float) (math::abs(evt.mousepos.y - evt.dragStart.y));
                    if (initialx + initialy < 4)
                        return;

                    if (initialx > initialy)
                        dragDirection = 1;
                    else
                        dragDirection = 0;
                }
                isMove = dragDirection == 0;
            }
            float distx = (float) ((evt.dragDistance->x));
            float disty = (float) ((evt.dragDistance->y));
            if ((!lockGesture && math::abs(disty) > 0) || (lockGesture && isMove)) {
                evt.dragDistance->y = 0;
                setOffset(layoutRoll.offset() + disty);
            }
            if ((!lockGesture && math::abs(distx) > 0) || (lockGesture && !isMove)) {
                evt.dragDistance->x = 0;
                setScale(layoutRoll.scale() + distx * 0.05f);
                int32_t rel  = math::min(size.y - 1, math::max(0, size.y - evt.relMousepos.y));
                float offset = (size.y - toScreenF(dragPosObjSpace)) + layoutRoll.offset();
                setOffset(offset - rel);
            }
        }
    } else if (dragMode == dragmode::drag_piano_key) {
        ivec2 local = evt.relMousepos;
        vec2 note   = getNoteFromPos(local);
        if (note.y >= 0 && note.y < MAX_OCTAVES * 12) {
            int32_t notePitch = math::floorfS32(note.y);
            if (lastNote != notePitch) {
                ThreadLock lock = dawCtrl->lockPlayThread();
                if (lastNote > -1) {
                    int32_t killTime = midihost::getInstance()->killNote(-1, 0, lastNote);
                    if (killTime < lastNoteTime) {
                        log_printf("kill time < lastnote start:  %d < %d\n", killTime, lastNoteTime);
                        dbgassert(0);
                    }
                }
                lastNote     = notePitch;
                lastNoteTime = midihost::getInstance()->triggerNote(-1, 0, notePitch, toVel(note));
            }
        }
    }
}

void gui_pianoroll::handleDraggedRelease(MouseEvent& evt) {
    dragMode = dragmode::drag_none;
    if (lastNote > -1) {
        ThreadLock lock = dawCtrl->lockPlayThread();
        midihost::getInstance()->killNote(-1, 0, lastNote);
    }
    lastNote = -1;
}

void gui_pianoroll::handleRightClick(MouseEvent& evt) {
}

bool gui_pianoroll::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        evt.requestFocus(this);
        return true;
    }
    return false;
}
vec2 gui_pianoroll::getNoteFromPos(vec2 localPos) {
    float keyVelocity = (localPos.y / (float) size.x) * 1024;
    float keyOffset   = (size.y - 1 - localPos.y + layoutRoll.offset()) / layoutRoll.scale();
    int32_t pitch     = -1;
    if (keyOffset >= 0 && keyOffset < MAX_OCTAVES * 12) {
        pitch = toNoteF(localPos.y);
    }
    return { keyVelocity, pitch };
}

void gui_pianoroll::layout() {
    keysX     = size.x / 2;
    widthKeys = size.x - keysX;
}
void gui_pianoroll::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    float h         = size.y;
    auto labelColor = parent->theme->getContrastColor(GuiColor::COL_BG_BRT);
    nvgBeginPath(vg);
    nvgRect(vg, keysX, -4, widthKeys, size.y + 8);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_WHITE));
    nvgFill(vg);

    bool fold        = layoutRoll.bFoldNotes;
    float offset     = layoutRoll.offset();
    float scale      = layoutRoll.scale();
    int32_t firstKey = math::max((int32_t) floorf(offset / scale), 0);

    //render one extra key on top and bottom to fix antialiasing on edge of container
    if (firstKey > 0) {
        firstKey--;
    }

    float yOff      = offset - firstKey * scale - scale;
    ivec2 noteMouse = { -1, -1 };
    if (dragMode == dragmode::drag_none || dragMode == dragmode::drag_piano_key) {
        ivec2 imouse = toControlsObjectSpace(dawCtrl->m_mousePos, this);
        bool mouseIn = dawCtrl->getGuiOverRef() == toRef() && contains(imouse + pos) && imouse.x >= keysX;
        if (mouseIn) {
            vec2 note = getNoteFromPos(imouse);
            if (note.y >= 0 && note.y < MAX_OCTAVES * 12) {
                noteMouse = { math::roundfS32(note.x), math::floorfS32(note.y) };
            }
        }
    }

    std::vector<int32_t> notePlayingPitch;
    std::vector<int32_t> noteRealtimePitch;

    auto track = this->view.track();
    if (track) {

        ThreadLock lock = track->audio->midiMutex.lockThread();
        for (auto& note: track->audio->m_heldNotes) {
            if (note.isRealtime()) {
                if (!STL_CONTAINS(noteRealtimePitch, note.pitch)) {
                    noteRealtimePitch.push_back(note.pitch);
                }
            } else {
                if (!STL_CONTAINS(notePlayingPitch, note.pitch)) {
                    notePlayingPitch.push_back(note.pitch);
                }
            }
        }
    }
    if (fold) {
        std::vector<int32_t> pitches;
        this->view.getNotePitches(pitches);

        nvgSave(vg);
        nvgTranslate(vg, 0, yOff);

        int len = pitches.size();
        nvgBeginPath(vg);
        float y = 0;
        for (int i = firstKey; i < len; i++) {
            int32_t pitch = pitches[i];
            if (isSharp(pitch)) {
                nvgRect(vg, keysX, h - y, widthKeys, scale);
            }
            y += scale;
            if (y >= size.y + scale * 2) {
                break;
            }
        }
        nvgFillColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_BLACK));
        nvgFill(vg);
        if (!noteRealtimePitch.empty()) {
            nvgBeginPath(vg);

            //TODO: iterate noteRealtimePitch, not pitches
            y = 0;
            for (int i = firstKey; i < len; i++) {
                int32_t pitch = pitches[i];
                if (STL_CONTAINS(noteRealtimePitch, pitch)) {
                    nvgRect(vg, keysX, h - y, widthKeys, scale);
                }
                y += scale;
                if (y >= size.y + scale * 2) {
                    break;
                }
            }
            nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_REALTIME));
            nvgFill(vg);
        }
        if (!notePlayingPitch.empty()) {
            nvgBeginPath(vg);
            y = 0;
            for (int i = firstKey; i < len; i++) {
                int32_t pitch = pitches[i];
                if (STL_CONTAINS(notePlayingPitch, pitch)) {
                    nvgRect(vg, keysX, h - y, widthKeys, scale);
                }
                y += scale;
                if (y >= size.y + scale * 2) {
                    break;
                }
            }
            nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_PLAYING));
            nvgFill(vg);
        }
        int32_t idx = noteMouse.y < 0 ? -1 : indexOfCtr(pitches, noteMouse.y);
        if (idx >= 0) {
            y = (idx - firstKey) * scale;
            if (y < size.y + scale * 2) {//should always be true, unclickable otherwise
                nvgBeginPath(vg);
                nvgRect(vg, keysX, h - y, widthKeys, scale);
                nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_MOUSE));
                nvgFill(vg);
            }
        }

        nvgBeginPath(vg);

        nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
        int lastOctave = -1;
        y = 0;
        for (int i = firstKey; len > 0 && i <= len; i++) {
            int noteOctave = pitches[math::clamp(i, 0, len - 1)] / 12;
            if (i == firstKey || i == len || lastOctave != noteOctave) {
                int wSep = 55;
                nvgMoveTo(vg, keysX - wSep, h - y + scale);
                nvgLineTo(vg, keysX, h - y + scale);
            }
            lastOctave = noteOctave;
            nvgMoveTo(vg, keysX, h - y + scale);
            nvgLineTo(vg, keysX + widthKeys, h - y + scale);
            y += scale;
            if (y >= size.y + scale * 2) {
                break;
            }
        }
        nvgStroke(vg);
        float FONT_SIZE = 24.0f;
        while (FONT_SIZE > 6.0f && scale <= FONT_SIZE) {
            FONT_SIZE -= 2.0f;
        }
        setFont(vg, FONT_SIZE, labelColor, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        y = 0;
        for (int i = firstKey; scale > FONT_SIZE && len > 0 && i < len; ++i) {
            int32_t pitch      = pitches[math::clamp(i, 0, len - 1)];
            const char* notename = noteName(pitch);
            float textY          = h - y + scale - (math::clamp<float>(scale, FONT_SIZE, 32)) / 2.0;
            nvgText(vg, 4, textY, notename, NULL);
            y += scale;
        }
        nvgRestore(vg);
    } else {
        nvgSave(vg);

        int32_t firstOctave = floorf(firstKey / 12.0f);
        firstKey            = firstKey % 12;

        nvgTranslate(vg, 0, yOff);

        NVGpaint paint;
        memset(&paint, 0, sizeof(paint));
        paint.image      = -1;
        paint.innerColor = theme->getColor(GuiColor::COL_PIANOROLL_BLACK);
        paint.outerColor = paint.innerColor;
        paint.customPar  = NVGBatchedShading::NVG_BATCHED_SHADED;

        float yoct = 0;
        for (int32_t octave = firstOctave; octave < MAX_OCTAVES; octave++) {
            float y = yoct;
            //      nvgBeginPath(vg);
            int nRendered = 0;
            for (int i = firstKey; i < 12; i++) {
                if (isSharp(i)) {
                    nvgBatchedRect(vg, keysX, h - y, widthKeys, scale);
                    //          nvgRect(vg, keysX, h-y, widthKeys, scale);
                    nRendered++;
                }
                y += scale;
                if (y >= size.y + scale * 2) {
                    break;
                }
            }
            if (nRendered) {
                nvgFillPaint(vg, paint);
                nvgBatchedRender(vg);
            }
            //      nvgFillColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_BLACK));
            //      nvgFill(vg);


            if (!noteRealtimePitch.empty()) {
                y = yoct;
                nvgBeginPath(vg);
                //TODO: iterate noteRealtimePitch, not pitches
                for (int i = firstKey; i < 12; i++) {
                    if (STL_CONTAINS(noteRealtimePitch, octave * 12 + i)) {
                        nvgRect(vg, keysX, h - y, widthKeys, scale);
                    }
                    y += scale;
                    if (y >= size.y + scale * 2) {
                        break;
                    }
                }
                nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_REALTIME));
                nvgSetShapeExtents(vg, keysX, -4, widthKeys, size.y + 8);
                nvgFill(vg);
            }
            if (!notePlayingPitch.empty()) {
                y = yoct;
                nvgBeginPath(vg);
                //TODO: iterate notePlayingPitch, not pitches
                for (int i = firstKey; i < 12; i++) {
                    if (STL_CONTAINS(notePlayingPitch, DAW::ToNoteNumber(octave, i))) {
                        nvgRect(vg, keysX, h - y, widthKeys, scale);
                    }
                    y += scale;
                    if (y >= size.y + scale * 2) {
                        break;
                    }
                }
                nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_PLAYING));
                nvgSetShapeExtents(vg, keysX, -4, widthKeys, size.y + 8);
                nvgFill(vg);
            }
            if (noteMouse.y >= 0 && noteMouse.y / 12 == octave) {
                y = yoct + ((noteMouse.y % 12) - firstKey) * scale;
                if (y < size.y + scale * 2) {//should always be true, unclickable otherwise
                    nvgBeginPath(vg);
                    nvgRect(vg, keysX, h - y, widthKeys, scale);
                    nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_MOUSE));
                    nvgSetShapeExtents(vg, keysX, h - y, widthKeys, scale);
                    nvgFill(vg);
                }
            }


            y = yoct;

            nvgBeginPath(vg);
            nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
            if (firstKey == 0) {
                nvgMoveTo(vg, keysX - 55, h - (y - scale));
                nvgLineTo(vg, keysX, h - (y - scale));
            }
            if (firstKey == 0 && octave == 0) {
                nvgMoveTo(vg, keysX, h - (y - scale));
                nvgLineTo(vg, keysX + widthKeys, h - (y - scale));
            }
            nvgStroke(vg);

            nvgBeginPath(vg);
            for (int i = firstKey; i < 12; i++) {
                nvgMoveTo(vg, keysX, h - y);
                nvgLineTo(vg, keysX + widthKeys, h - y);
                y += scale;
                if (y >= size.y + scale * 2) {
                    break;
                }
            }
            nvgStroke(vg);
            yoct = y;
            if (yoct >= size.y + scale * 2) {
                break;
            }
            firstKey = 0;
        }
        nvgRestore(vg);
        setFont(vg, 24, labelColor, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        char buf[5];
        for (int32_t octave = 0; octave < MAX_OCTAVES; octave++) {
            float y     = scale * octave * 12;
            float textY = h - y + offset;
            if (textY > size.y + scale + 20 || textY < -20) {
                continue;
            }

            snprintf(buf, sizeof(buf), "C%d", octave - 2);
            nvgText(vg, 4, textY, buf, NULL);
        }
    }
    nvgBeginPath(vg);
    nvgMoveTo(vg, keysX + widthKeys, 0);
    nvgLineTo(vg, keysX + widthKeys, size.y);
    nvgStrokeWidth(vg, theme->getFloat(GuiConstant::CONST_PIANOROLL_STROKE_WIDTH));
    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PIANOROLL_STROKE));
    nvgStroke(vg);
}
