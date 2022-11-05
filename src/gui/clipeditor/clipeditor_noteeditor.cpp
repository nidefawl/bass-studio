#include <algorithm>
#include <cstdio>
#include <memory>
#include <nanovg.h>
#include "assert_dbg.h"
#include "clipeditor.h"

#include "color_util.h"
#include "gui/track/trackctr.h"
#include "math/seq_math.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "plugins/synth/IPlugMidi.h"
#include "seq_util.h"
#include "str_util.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "note.h"
#include "seq_time.h"
#include "cursor.h"
#include "keyboard.h"
#include "grid.h"
#include "types.h"
#include "wave/waveform_render.h"
#include "wave/waveform_render_impl.h"

#include "gui/contextmenu/contextmenu_daw.h"
#include "logging.h"


static constexpr int32_t CLIPEDITOR_DEFAULT_MIN = DAW::ToNoteNumber(1, 0);
static constexpr int32_t CLIPEDITOR_DEFAULT_MAX = DAW::ToNoteNumber(3, 0);

void guictr_cliphandles::handleDraggedBegin(MouseEvent& evt) {
    dragHandle  = dragModeMouseOver; //getDragZone(local);
    dragModeMouseOver = drag_handle_none;
    clip_t* clip = view.clip();
    if (!clip) {
        dragHandle   = drag_handle_none;
        return;
    }
    if (dragHandle == drag_handle_none) {
        parentEditor.getClipEditor().selectEditClip(view.gui);
        return;
    }
    dragOffset  = evt.relMousepos.x - (int32_t) (grid.tickToScreenD(getTickOffset() + clip->loopStart));
}

void guictr_cliphandles::handleDraggedMove(MouseEvent& evt) {
    clip_t* clip = view.clip();
    if (!clip)
        return;

    track_t* track = view.track();
    if (!track)
        return;

    if (dragHandle == drag_handle_none) {
        return;
    }
    ThreadLock lock = dawCtrl->lockPlayThread();
    trackdata_midi_t& midi = track->getMidi();
    clip_t* clNext = midi.getNextClip(clip);
    dbgassert(!clNext || (clNext != clip));
    dbgassert(!clNext || clNext->start() >= clip->end());
    int32_t mousePosX = evt.relMousepos.x;
    if (dragHandle == drag_handle_loopbar) {
        mousePosX -= dragOffset;
    }
    tick_t tickRelative = grid.screenToTickSnap(mousePosX, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON) - getTickOffset();
    tick_t clipEndOffset     = clip->offsetStart + clip->getLen();
    tick_t curLoopEnd = clip->loopStart + clip->loopLen;
    if (dragHandle == drag_handle_right) {
        if (!parentEditor.getClipView().isAbsoluteTimeMode()) {
            tick_t tickDelta = (tickRelative - clipEndOffset);
            tick_t newLen    = clip->getLen() + tickDelta;
            tick_t newEnd = clip->start() + newLen;
            if (clNext && newEnd >= clNext->start()) {
                newLen = clNext->start() - clip->start();
            }
            if (newLen <= 0 || newLen == clip->getLen()) {
                return;
            }
            clip->setLen(newLen);
        } else {
            tick_t newEnd = clip->start() + tickRelative;
            if (clNext && newEnd >= clNext->start()) {
                newEnd = clNext->start();
            }
            if (newEnd <= clip->start() || newEnd == clip->end()) {
                return;
            }
            clip->setLen(newEnd - clip->start());
        }
    }
    if (dragHandle == drag_handle_left) {
        if (!parentEditor.getClipView().isAbsoluteTimeMode()) {
            tick_t curStart  = clip->offsetStart;
            tick_t tickDelta = (tickRelative - curStart);
            tick_t newStart  = clip->offsetStart + tickDelta;
            if (newStart < clipEndOffset) {
                tick_t newLen = clipEndOffset - newStart;
                tick_t newEnd = clip->start() + newLen;
                if (clNext && newEnd >= clNext->start()) {
                    auto newLen = clNext->start() - clip->start();
                    if (!newLen) return;
                    clip->setLen(newLen);
                    clip->offsetStart = clipEndOffset - clip->getLen();
                } else {
                    clip->offsetStart = newStart;
                    auto newLen = clipEndOffset - newStart;
                    if (!newLen) return;
                    clip->setLen(newLen);
                }
            }
        } else {
            clip_t* clPrev = midi.getPrevClip(clip);
            tick_t newStart  = clip->start() + tickRelative;
            auto clipEnd = clip->end();
            if (newStart < 0) {
                newStart = 0;
            }
            if (newStart >= clipEnd) {
                newStart = clipEnd - 1;
            }
            if (clPrev && newStart < clPrev->end()) {
                newStart = clPrev->end();
            }
            if (clip->time == newStart)
                return;
            clip->time = newStart;
            clip->len = clipEnd - newStart;
        }
    }
    if (dragHandle == drag_handle_loopright) {
        tick_t tickDelta = (tickRelative - curLoopEnd);
        tick_t newLen    = clip->loopLen + tickDelta;
        if (newLen > 0) {
            if (clip->loopLen == newLen)
                return;
            clip->loopLen = newLen;
        }
    }
    if (dragHandle == drag_handle_loopleft) {
        tick_t curLoopStart = clip->loopStart;
        tick_t tickDelta    = (tickRelative - curLoopStart);
        tick_t newStart     = clip->loopStart + tickDelta;
        if (newStart < curLoopEnd) {
            if (clip->loopStart == newStart && clip->loopLen == curLoopEnd - newStart)
                return;
            clip->loopStart = newStart;
            clip->loopLen   = curLoopEnd - newStart;
        }
    }
    if (dragHandle == drag_handle_loopbar) {
        tick_t curLoopStart = clip->loopStart;
        tick_t tickDelta    = (tickRelative - curLoopStart);
        if (!tickDelta)
            return;
        clip->loopStart += tickDelta;
        clip->offsetStart = clip->loopStart;
    }
    clip->setDirty();
    dawCtrl->getDaw()->updateVisibleTrackContents();
}

void guictr_cliphandles::handleDraggedRelease(MouseEvent& evt) {
    dragHandle = drag_handle_none;
}

guictr_cliphandles::dist_dragzone_handle guictr_cliphandles::getDragZone(ivec2 local) {
    if (view.clip()) {

        float halfHeight = size.y / 2.0f;
        float dragTop    = halfHeight * 0.5f;
        float dragBottom = halfHeight + halfHeight * 0.5f;
        float distBar    = std::numeric_limits<float>::max();
        float barSX      = clipLoopStartScrX();
        float barEX      = clipLoopEndScrX();
        if (local.x >= barSX && local.x < barEX && local.y >= halfHeight && local.y < halfHeight * 2) {
            distBar = DRAG_RANGE * DRAG_RANGE * 0.8f;
        }
        std::array<dist_dragzone, 5> hndls{
            dist_dragzone{ dist(clipStartScrX()+3, dragTop, local), dragmode::drag_handle_left },
            { dist(clipEndScrX()-3, dragTop, local), dragmode::drag_handle_right },
            { dist(barSX+3, dragBottom, local), dragmode::drag_handle_loopleft },
            { dist(barEX-3, dragBottom, local), dragmode::drag_handle_loopright },
            { distBar, dragmode::drag_handle_loopbar }
        };
        if (!bIsHandleActive) {
            for (auto& hndl : hndls) {
                if (hndl.mode == dragmode::drag_handle_loopbar
                    || hndl.mode == dragmode::drag_handle_loopleft
                    || hndl.mode == dragmode::drag_handle_loopright) {
                    hndl.mode = dragmode::drag_handle_none;
                }
            }
        }
        std::sort(hndls.begin(), hndls.end(), [](dist_dragzone const& a, dist_dragzone const& b) {
            if (a.mode == dragmode::drag_handle_none)
                return false;
            if (b.mode == dragmode::drag_handle_none)
                return true;
            return a.dist < b.dist;
        });
        if (hndls[0].dist < DRAG_RANGE * DRAG_RANGE) {
            return {hndls[0], this};
        }
    }
    return { {std::numeric_limits<float>::max(), dragmode::drag_handle_none}, nullptr };
}

bool guictr_cliphandles::containsHandlePos(ivec2 mpos) const {
    if (bIsHandleActive) {
        float barSX      = clipLoopStartScrX();
        float barEX      = clipLoopEndScrX();
        float halfHeight = size.y / 2.0f;
        if (mpos.x >= barSX && mpos.x < barEX && mpos.y >= halfHeight && mpos.y < halfHeight * 2) {
            return true;
        }
    }
    return mpos.x >= clipStartScrX() && mpos.x < clipEndScrX();
}

bool guictr_cliphandles::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 local = this->toContainerSpace(mpos);
        if (view.clip() && evt.type <= MouseHitType::MOUSE_RIGHT) {
            auto absTimeMode = parentEditor.getClipView().isAbsoluteTimeMode();
            if (absTimeMode && containsHandlePos(local)) {
                dragModeMouseOver = drag_handle_none;
                evt.requestFocus(this);
                return true;
            }
        }
    }
    return false;
}
void guictr_cliphandles::renderLoopHandle(NVGcontext* vg, vec2 editorSize) const {
    auto clip = view.clip();
    if (!clip) {
        return;
    }
    NVGcolor colLI        = theme->getColor(GuiColor::COL_LOOPHANDLES);
    const NVGcolor colLIStroke  = theme->getFrameColorOutline();
    const float strokeWidthLI   = theme->getFloat(GuiConstant::CONST_CLIPEDITOR_HANDLES_STROKE_WIDTH);
    const auto heightLoopInidicator = float(size.y*0.5f);
    const float wLoopInidicator = heightLoopInidicator;
    if (!bIsHandleActive) {
        colLI.r *= 0.5f;
        colLI.g *= 0.5f;
        colLI.b *= 0.5f;
    }
    float tickBeginX = clipStartScrX();
    float tickEndX   = clipEndScrX();

    float yOffset = 0;
    auto cs = editorSize;
    yOffset += heightLoopInidicator;
    tickBeginX = clipLoopStartScrX();
    tickEndX   = clipLoopEndScrX();
    if (!(tickBeginX - wLoopInidicator > cs.x || tickEndX + wLoopInidicator < 0)) {
        float barBeginX = math::max(-wLoopInidicator, tickBeginX);
        float barEndX   = math::min(cs.x + wLoopInidicator, tickEndX);
        nvgBeginPath(vg);
        nvgRect(vg, barBeginX, yOffset, barEndX - barBeginX, heightLoopInidicator);

        nvgFillColor(vg, colLI);
        nvgFill(vg);
        nvgStrokeColor(vg, colLIStroke);
        nvgStrokeWidth(vg, strokeWidthLI);
        nvgStroke(vg);

        if (tickBeginX > -wLoopInidicator && tickBeginX < cs.x + wLoopInidicator) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, tickBeginX, yOffset);
            nvgLineTo(vg, tickBeginX, yOffset + cs.y);
            nvgStrokeColor(vg, colLI);
            nvgStrokeWidth(vg, strokeWidthLI);
            nvgStroke(vg);
            drawTri(vg, tickBeginX, yOffset, wLoopInidicator, 0, colLI, colLIStroke, strokeWidthLI);
        }

        if (tickEndX > -wLoopInidicator && tickEndX < cs.x + wLoopInidicator) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, tickEndX, yOffset);
            nvgLineTo(vg, tickEndX, yOffset + cs.y);
            nvgStrokeColor(vg, colLI);
            nvgStrokeWidth(vg, strokeWidthLI);
            nvgStroke(vg);
            drawTri(vg, tickEndX, yOffset, wLoopInidicator, 1, colLI, colLIStroke, strokeWidthLI);
        }
    }
    yOffset += heightLoopInidicator;
}
void guictr_cliphandles::renderHandle(NVGcontext* vg, int32_t trackSelIdx) const {
    auto clip = view.clip();
    if (!clip) {
        return;
    }
    NVGcolor colLI        = theme->getColor(GuiColor::COL_LOOPHANDLES);
    const NVGcolor colLIStroke  = theme->getFrameColorOutline();
    const float strokeWidthLI   = theme->getFloat(GuiConstant::CONST_CLIPEDITOR_HANDLES_STROKE_WIDTH);
    const auto heightLoopInidicator = float(size.y*0.5f);
    const float wLoopInidicator = heightLoopInidicator;
    if (trackSelIdx != this->trackSelectionIdx) {
        colLI.r *= 0.5f;
        colLI.g *= 0.5f;
        colLI.b *= 0.5f;
    }
    float tickBeginX = clipStartScrX();
    float tickEndX   = clipEndScrX();

    float yOffset = 0;
    auto cs = size;
    if (!(tickBeginX - wLoopInidicator > cs.x || tickEndX + wLoopInidicator < 0)) {
        float barBeginX = math::max(-wLoopInidicator, tickBeginX);
        float barEndX   = math::min(cs.x + wLoopInidicator, tickEndX);
        NVGcolor color  = rgbToNvg(view.clip()->rgb);
        nvgBeginPath(vg);
        nvgRect(vg, barBeginX, yOffset, barEndX - barBeginX, heightLoopInidicator * 2);
        nvgFillColor(vg, color);
        nvgFillCustomPar(vg, -4);
        nvgFill(vg);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CLIP_OUTLINE));
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
    }
    if (tickBeginX > -wLoopInidicator && tickBeginX < cs.x + wLoopInidicator) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, tickBeginX, yOffset);
        nvgLineTo(vg, tickBeginX, yOffset + cs.y);
        nvgStrokeColor(vg, colLI);
        nvgStrokeWidth(vg, strokeWidthLI);
        nvgStroke(vg);
        drawTri(vg, tickBeginX, yOffset, heightLoopInidicator, 0, colLI, colLIStroke, strokeWidthLI);
    }

    if (tickEndX > -wLoopInidicator && tickEndX < cs.x + wLoopInidicator) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, tickEndX, yOffset);
        nvgLineTo(vg, tickEndX, yOffset + cs.y);
        nvgStrokeColor(vg, colLI);
        nvgStrokeWidth(vg, strokeWidthLI);
        nvgStroke(vg);
        drawTri(vg, tickEndX, yOffset, heightLoopInidicator, 1, colLI, colLIStroke, strokeWidthLI);
    }
}
/* render track-editor selection range in clipview */
void renderSelectionIndicator(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid , ivec2 handlesPos, ivec2 handlesSize, const clip_t* viewClip, const DAW::Cursor& c, float heightSelIndicator) {
    nvgBeginPath(vg);
    nvgRect(vg, handlesPos.x - 2, handlesPos.y, handlesSize.x + 4, heightSelIndicator);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
    nvgFill(vg);
    if (c.selRange) {
        auto clipOffset = (viewClip) ? viewClip->getOffsetStart() : 0;
        auto tickBegin = c.getTickBegin() - clipOffset;
        auto tickEnd   = c.getTickEnd() - clipOffset;
        auto tickBeginX  = grid.tickToScreenD(tickBegin);
        auto tickEndX    = grid.tickToScreenD(tickEnd);
        if (tickEndX > -4.0 && tickBeginX < handlesSize.x + 4.0) {
            auto width = tickEndX - tickBeginX;
            if (width < 0.5f) {
                return;
            }
            auto inset = math::max(1.0f, heightSelIndicator * 0.25f);
            nvgBeginPath(vg);
            nvgRect(vg, handlesPos.x + tickBeginX, handlesPos.y + inset, width, heightSelIndicator - inset * 2.0f);
            nvgFillColor(vg, theme->getColor(GuiColor::COL_NOTE_SELECTED));
            nvgFill(vg);
        }
    }
}

void renderPlayHead(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid, ivec2 handlesPos, ivec2 handlesSize, const clip_t* viewClip, tick_t playbackPos, bool bIsAbsoluteTime, float fWidth) {
    if (viewClip) {
        tick_t tickPos = playbackPos;
        if (!bIsAbsoluteTime) {
            tickPos -= viewClip->time + viewClip->offsetStart;
            if (viewClip->loopEnabled && viewClip->loopLen > 0) {
                if (tickPos > viewClip->loopStart) {
                    tickPos = viewClip->loopStart + (tickPos - viewClip->loopStart) % viewClip->loopLen;
                }
            }
        }
        float playBackX = (float) grid.tickToScreenD(tickPos);
        if (playBackX > -4.0f && playBackX < handlesSize.x + 4.0f) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, handlesPos.x + playBackX, handlesPos.y);
            nvgLineTo(vg, handlesPos.x + playBackX, handlesSize.y);
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLAYHEAD_OUTLINE));
            nvgStrokeWidth(vg, 3 * fWidth);
            nvgStroke(vg);
            nvgBeginPath(vg);
            nvgMoveTo(vg, handlesPos.x + playBackX, handlesPos.y);
            nvgLineTo(vg, handlesPos.x + playBackX, handlesPos.y + handlesSize.y);
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLAYHEAD));
            nvgStrokeWidth(vg, 1 * fWidth);
            nvgStroke(vg);
        }
    }
}
void guictr_cliphandles::render(NVGcontext* vg) {
    nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
    nvgTranslate(vg, pos.x, pos.y);
    renderHandle(vg, 0);
}

guictr_noteeditor::guictr_noteeditor(guictr_clipeditor& parentClipEditor, clip_view& _view)
    : guictr_editor_base(parentClipEditor, _view), layout_pianoroll_t(),
      piano(_view, *this),
      content(grid, _view, *this),
      velocities(grid, _view, *this),
      ctrlData(grid, _view, *this),
      splitterVel(0, 0.75f)
{
    splitterVel.setMinMax(0.5f, 0.95f);
    splitterVel.setCallback(this);
    padding = 2;
    grid.showRange(0, TICKS_BAR * 4);
    grid.addCallback(this);
    add(&piano);
    add(&content);
    add(&ctrlData);
    add(&velocities);
    add(&timeline);
    // add(&clipHandles);
    add(&btnToggleFold);
    add(&btnToggleVelocities);
    add(&btnToggleControlData);
    add(&dropdownSelectControlData);
    add(&splitterVel);
    btnToggleFold.setButtonColor(GuiColor::COL_FOLD_BUTTON);
    btnToggleFold.setText("Fold");
    btnToggleFold.setStateRef(&fold);
    btnToggleVelocities.setText("Velocities");
    btnToggleControlData.setText("Control Data");
    dropdownSelectControlData.setText("Select");
    dropdownSelectControlData.fnOptionSelected = [this](int32_t option, String& str) {
        ctrlData.setSelectedData(option);
        return str;
    };
    std::vector<String> options = {
        "Pitch Bend" };
    for (int32_t i = 1; i < 127; ++i) {
        String s = IMidiMsg::ControlName(i);
        options.push_back(s);
    }
    dropdownSelectControlData.setOptions(options);
    dropdownSelectControlData.setSelectedIndex(ctrlData.getSelectedData());
    content.showRange(CLIPEDITOR_DEFAULT_MIN, CLIPEDITOR_DEFAULT_MAX);
}

guictr_noteeditor::~guictr_noteeditor() {
    remove(&splitterVel);
    remove(&dropdownSelectControlData);
    remove(&btnToggleControlData);
    remove(&btnToggleVelocities);
    remove(&btnToggleFold);
    remove(&timeline);
    remove(&ctrlData);
    remove(&velocities);
    remove(&content);
    remove(&piano);
    removeGuis();
    // remove(&clipHandles);
}

void guictr_noteeditor::handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) {
    if (clampedAt == 1) {
        ctrlData.setVisible(false);
    } else {
        ctrlData.setVisible(true);
    }
    layout();
}


void guictr_noteeditor::buttonClicked(guibase* button) {
    if (button == &btnToggleFold) {
        fold = !fold;
        view.updateNotePitches(true);
        if (fold && yscalefold == 0 && yoffsetfold == 0) {
            zoomPianoRollToClipsNoteRange();
        }
    }
    if (button == &btnToggleVelocities) {
        bool bShowVel = this->velocities.isVisible();
        this->velocities.setVisible(!bShowVel);
        this->layout();
    }
    if (button == &btnToggleControlData) {
        bool bShowCtrl = this->ctrlData.isVisible();
        this->ctrlData.setVisible(!bShowCtrl);
        this->layout();
    }
}

void guictr_noteeditor::renderBackground(NVGcontext* vg) {
    drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
}

void guictr_noteeditor::layout() {
    ivec2 cs = getSizeContent();

    auto heightContent = cs.y - heightTimeLine - handlesHeight;
    velHeight = math::clamp(splitterVel.rightOrBottom(heightContent), 0, 220);
    if (!velocities.isVisible()) {
        velocities.size = ivec2(cs.x - pianoWidth, 0);
    } else {
        heightContent -= velHeight;
        velocities.size = ivec2(cs.x - pianoWidth, velHeight);
    }
    if (!ctrlData.isVisible()) {
        ctrlData.size = ivec2(cs.x - pianoWidth, 0);
    } else {
        heightContent -= velHeight;
        ctrlData.size = ivec2(cs.x - pianoWidth, velHeight);
    }
    piano.size      = ivec2(pianoWidth, heightContent);
    piano.pos          = ivec2(0, heightTimeLine + handlesHeight);
    timeline.pos       = ivec2(piano.right(), 0);
    timeline.size      = ivec2(cs.x - pianoWidth, heightTimeLine);
    auto clipHandlesPos = ivec2(timeline.left(), timeline.bottom() + heightSelIndicator);
    auto insetClipHandleY = 4;
    for (auto& clipHandles : clipsHandles) {
        clipHandles->pos  = clipHandlesPos + ivec2(0, heightLoopInidicator * 2 * clipHandles->getTrackSelectionIdx() + insetClipHandleY);
        clipHandles->size = ivec2(timeline.size.x, heightLoopInidicator * 2 - insetClipHandleY * 2);
    }
    content.pos        = ivec2(timeline.left(), timeline.bottom() + handlesHeight);
    content.size       = ivec2(timeline.size.x, piano.size.y);
    velocities.pos     = ivec2(timeline.left(), content.bottom());
    ctrlData.pos     = ivec2(timeline.left(), velocities.bottom());
    splitterVel.pos    = piano.getLeftBottom() - Splitter::SPLITTER_LAYOUT_THICKNESS / 2;
    splitterVel.size   = ivec2(cs.x, Splitter::SPLITTER_LAYOUT_THICKNESS);
    btnToggleFold.pos  = ivec2(padding, padding);
    btnToggleFold.size = ivec2((pianoWidth) / 2, 18);
    btnToggleVelocities.pos = btnToggleFold.getRightTop();
    btnToggleVelocities.size = ivec2((pianoWidth) / 2, 18);
    btnToggleControlData.pos = btnToggleFold.getLeftBottom();
    btnToggleControlData.size = ivec2((pianoWidth) / 2, 18);
    dropdownSelectControlData.setVisible(ctrlData.isVisible());
    if (dropdownSelectControlData.isVisible()) {
        dropdownSelectControlData.pos = ctrlData.getLeftTop() - ivec2(pianoWidth, 0);
        dropdownSelectControlData.size = ivec2(pianoWidth, 18);
    }
    
    grid.update(content.size);
    for (guibase* gui: guis) {
        gui->layout();
    }
}

void guictr_noteeditor::gridChanged(scaled_grid& _grid) {
    ivec2 cs = getSizeContent();
    _grid.update(ivec2(timeline.size.x, cs.y - 30));
}

void guictr_noteeditor::handleDraggedBegin(MouseEvent& evt) {
    if (evt.guiDragged == &piano) {
        if (evt.type == M_EVT_DOUBLECLICK)
            zoomPianoRollToClipsNoteRange();

        return;
    }
}

void guictr_noteeditor::zoomPianoRollToClipsNoteRange() {
    clip_t* clip = view.clip();
    if (!clip || clip->isEmpty()) {
        content.showRange(CLIPEDITOR_DEFAULT_MIN, CLIPEDITOR_DEFAULT_MAX);
        return;
    }
    int32_t minSemi = clip->notes.minNote.pitch;
    int32_t maxSemi = clip->notes.maxNote.pitch;
    if (fold) {
        minSemi = 0;
        maxSemi = view.notePitches.size();
    }
    int32_t range = math::abs(maxSemi - minSemi);
    if (range < 6) {
        int32_t add = 6 - range;
        minSemi -= add / 2;
        maxSemi += add / 2;
    } else {
        maxSemi += 2;
        minSemi -= 2;
    }
    content.showRange(minSemi, maxSemi);
}

void guictr_noteeditor::showEditClip() {
    clip_t* currentClip = view.clip();
    if (currentClip) {
        bool bIsAbsMode = view.isAbsoluteTimeMode();
        auto& layout = bIsAbsMode ? view.selectionView.editorLayout : currentClip->editorLayout;
        if (layout.noLayout) {
            if (bIsAbsMode) {
                grid.showRange(view.selectionView.minClipStart, view.selectionView.maxClipEnd);
            } else {
                grid.showRange(currentClip->offsetStart, currentClip->offsetStart + currentClip->getLen());
            }
            zoomPianoRollToClipsNoteRange();
        } else {
            grid.setLayout(layout.layoutGrid);
            setLayout(layout.layoutPianoRoll);
        }
    }
    ctrlData.showEditClip();
    auto newClipHandleCount = view.selectionView.totalClipCount;
    auto curClipHandleCount = clipsHandles.size();
    for (size_t i = newClipHandleCount; i < curClipHandleCount; i++) {
        clipsHandles[i]->setVisible(false);
        clipsHandles[i]->getClipView().reset();
    }
    for (size_t i = curClipHandleCount; i < newClipHandleCount; i++) {
        if (clipsHandles.size() <= i || !clipsHandles[i]) {
            clipsHandles.push_back(std::make_shared<guictr_cliphandles>(*this, grid));
            this->add(clipsHandles[i].get());
        }
    }
    auto numTracks = view.selectionView.tracks.size();
    handlesHeight = math::max<int32_t>(1, numTracks) * heightLoopInidicator * 2 + heightSelIndicator;
    bool foundThis = false;
    auto it = clipsHandles.begin();
    for (size_t trackIdx = 0; trackIdx < numTracks; trackIdx++) {
        auto& [trackEntry, vecTrackClips] = view.selectionView.tracks[trackIdx];
        auto numClipsOnTrack = vecTrackClips.size();
        for (size_t clipIdx = 0; clipIdx < numClipsOnTrack && it != clipsHandles.end(); clipIdx++) {
            auto& selClip = vecTrackClips[clipIdx];
            auto& clipHandles = **(it++);
            dbgassert(it <= clipsHandles.end());
            if (!assert_expr(trackEntry.clipsGuis.count(selClip))) {
                continue;
            }
            auto selGClip = trackEntry.clipsGuis[selClip];
            dbgassert(selGClip);
            dbgassert(selGClip->m_clip);
            dbgassert(selGClip->m_clip == selClip);
            clipHandles.setVisible(true);
            clipHandles.getClipView().set(selGClip, {});
            clipHandles.setTrackSelectionIdx(trackIdx);
            dbgassert(clipHandles.getClipView().clip() == selClip);
            clipHandles.setHandleActive(selClip == currentClip);
            moveToBegin(&clipHandles);
            if (clipHandles.isHandleActive()) {
                foundThis = true;
            }
        }
    }
    dbgassert(!currentClip || !view.selectionView.totalClipCount || foundThis);
    dbgassert(clipsHandles.size() >= view.selectionView.totalClipCount);
    for (size_t i = 0; i < view.selectionView.totalClipCount; i++) {
        dbgassert(clipsHandles[i]->isVisible());
        dbgassert(clipsHandles[i]->getClipView().clip());
    }
    for (size_t i = view.selectionView.totalClipCount; i < clipsHandles.size(); i++) {
        dbgassert(!clipsHandles[i]->isVisible());
        dbgassert(!clipsHandles[i]->getClipView().clip());
    }
}

void guictr_noteeditor::selectEditClip(gui_clip* gclip) {
    if (gclip != view.gui) {
        view.setSelected(gclip);
        if (!assert_expr(clipsHandles.size() >= view.selectionView.totalClipCount)) {
            return;
        }
        clip_t* currentClip = view.clip();
        auto numTracks = view.selectionView.tracks.size();
        auto it = clipsHandles.begin();
        for (size_t trackIdx = 0; trackIdx < numTracks; trackIdx++) {
            auto& [trackEntry, vecTrackClips] = view.selectionView.tracks[trackIdx];
            auto numClipsOnTrack = vecTrackClips.size();
            for (size_t clipIdx = 0; clipIdx < numClipsOnTrack && it != clipsHandles.end(); clipIdx++) {
                auto& selClip = vecTrackClips[clipIdx];
                auto& clipHandles = **(it++);
                dbgassert(it <= clipsHandles.end());
                if (!assert_expr(trackEntry.clipsGuis.count(selClip))) {
                    continue;
                }
                clipHandles.setHandleActive(selClip == currentClip);
                if (clipHandles.isHandleActive()) {
                    moveToBegin(&clipHandles);
                }
            }
        }
    }
}

void guictr_noteeditor::storeLayout() {
    clip_editor_layout_t& layout = view.selectionView.editorLayout;
    layout.layoutPianoRoll = *static_cast<layout_pianoroll_t*>(this);
    layout.layoutGrid = grid;//TODO: add a cast to get rid of slicing warning
    layout.noLayout   = false;
    clip_t* clip = view.clip();
    if (clip) {
        clip->editorLayout = layout;
    }
}

bool guictr_noteeditor::handleKeyInput(KeyEvent& kevt) {
    return content.handleKeyInput(kevt);
}
bool guictr_noteeditor::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    if (isCtrl(evt.kbmods)) {
        float zomDelta   = 1.0f + yoffset * -0.2f;
        ivec2 localMouse = timeline.toContainerSpace(evt.relMousepos);
        timeline.adjustZoom(localMouse.x, zomDelta);
    } else if (isShift(evt.kbmods)) {
        timeline.adjustOffset(-yoffset * 32);
    } else {
        piano.setOffset(offset() + yoffset * 2.0 * scale());
    }
    return true;
}

void guictr_noteeditor::setLayout(layout_pianoroll_t& layout) {
    yscale      = layout.yscale;
    yoffset     = layout.yoffset;
    yscalefold  = layout.yscalefold;
    yoffsetfold = layout.yoffsetfold;
    fold        = layout.fold;
}

void renderClipHandlesBackground(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid, vec2 handlesPos, vec2 handlesSize) {
    nvgBeginPath(vg);
    nvgRect(vg, handlesPos.x - 2, handlesPos.y, handlesSize.x + 2, handlesSize.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
    nvgFill(vg);
}
void renderGridList(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid, vec2 handlesPos, vec2 handlesSize) {
    for (grid_div g : grid.gridList) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, handlesPos.x + g.screenpos, handlesPos.y - 2);
        nvgLineTo(vg, handlesPos.x + g.screenpos, handlesPos.y + handlesSize.y + 2);
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
void guictr_noteeditor::render(NVGcontext* vg) {
    renderBackground(vg);
    if (!setScissorTransform(vg)) {
        return;
    }
    nvgSave(vg);
    piano.render(vg);
    nvgRestore(vg);
    nvgSave(vg);
    timeline.render(vg);
    nvgRestore(vg);
    nvgSave(vg);
    content.render(vg);
    nvgRestore(vg);
    if (velocities.isVisible()) {
        nvgSave(vg);
        velocities.render(vg);
        nvgRestore(vg);
    }
    if (ctrlData.isVisible()) {
        nvgSave(vg);
        ctrlData.render(vg);
        nvgRestore(vg);
    }
    auto playbackPos = dawCtrl->getDaw()->getPlaybackPos();
    auto clip = view.clip();
    auto handlesPos = timeline.getLeftBottom();
    auto handlesSize = ivec2(timeline.size.x, handlesHeight);
    nvgSave(vg);
    if (handlesSize.x > 5 && handlesSize.y > 5) {
        nvgIntersectScissor(vg, handlesPos.x, handlesPos.y, handlesSize.x, handlesSize.y);
        renderClipHandlesBackground(vg, theme, grid, vec2{0, heightSelIndicator}+vec2(handlesPos), handlesSize);
        int32_t trackIdx = 0;
        int32_t activeTrackIdx = -1;
        auto viewTrack = view.track();
        auto trackHandleHeight = (handlesHeight-heightSelIndicator) / math::max(1, CtrSize(view.selectionView.tracks));
        for (auto& [trackEntry, vecClips] : view.selectionView.tracks) {
                // nvgRect(vg, handlesPos.x, handlesPos.y + heightSelIndicator + trackHandleHeight * trackIdx+trackHandleHeight-heightSelIndicator, timeline.size.x, heightSelIndicator);
            auto col = trackIdx % 2 == 0 ? GuiColor::COL_GRID_DRK : GuiColor::COL_GRID_BRT;
            auto nvgCol = viewTrack == trackEntry.track ? rgbToNvg(trackEntry.track->rgb) : theme->getColor(col);
            nvgBeginPath(vg);
            nvgRect(vg, handlesPos.x, handlesPos.y + heightSelIndicator + trackHandleHeight * trackIdx + 2, timeline.size.x, trackHandleHeight-2);
            nvgCol.a *= 0.5f;
            nvgFillColor(vg, nvgCol);
            nvgFillCustomPar(vg, -1);
            nvgFill(vg);
            if (viewTrack == trackEntry.track) {
                activeTrackIdx = trackIdx;
            }
            trackIdx++;
        }
        renderGridList(vg, theme, grid, handlesPos, handlesSize);
        guictr_cliphandles* viewClipHandle = nullptr;
        if (!clipsHandles.empty() && clipsHandles.front()->isVisible()) {
            auto thizClip = view.clip();
            for (auto& handle: clipsHandles) {
                if (!handle->isVisible()) {
                    break;
                }
                auto clip = handle->getClipView().clip();
                if (clip == thizClip) {
                    viewClipHandle = handle.get();
                } else if (assert_expr(clip)) {
                    nvgTranslate(vg, handle->pos.x, handle->pos.y);
                    handle->renderHandle(vg, -1);
                    nvgTranslate(vg, -handle->pos.x, -handle->pos.y);
                }
            }
            if (viewClipHandle) {
                nvgTranslate(vg, viewClipHandle->pos.x, viewClipHandle->pos.y);
                viewClipHandle->renderHandle(vg, activeTrackIdx);
                nvgTranslate(vg, -viewClipHandle->pos.x, -viewClipHandle->pos.y);
            }
        }
        renderSelectionIndicator(vg, theme, grid, handlesPos, vec2(timeline.size.x, heightSelIndicator), view.isAbsoluteTimeMode()?nullptr:clip, dawCtrl->getCursor(), heightSelIndicator);
        // renderPlayHead(vg, theme, grid, handlesPos, handlesSize, clip, playbackPos, view.isAbsoluteTimeMode(), 1.0f);
        nvgRestore(vg);
        if (viewClipHandle) {
            nvgTranslate(vg, viewClipHandle->pos.x, viewClipHandle->pos.y);
            viewClipHandle->renderLoopHandle(vg, vec2(viewClipHandle->size.x, content.bottom() - viewClipHandle->top()));
            nvgTranslate(vg, -viewClipHandle->pos.x, -viewClipHandle->pos.y);
        }
        renderPlayHead(vg, theme, grid, content.pos, content.size, clip, playbackPos, view.isAbsoluteTimeMode(), 0.75f);
    }
    if (btnToggleFold.isVisible())
        btnToggleFold.render(vg);
    if (btnToggleVelocities.isVisible())
        btnToggleVelocities.render(vg);
    if (btnToggleControlData.isVisible())
        btnToggleControlData.render(vg);
    if (dropdownSelectControlData.isVisible())
        dropdownSelectControlData.render(vg);
    if (splitterVel.isVisible())
        splitterVel.render(vg);
}


gui_audiocontent::gui_audiocontent(scaled_grid& _grid, clip_view& _view)
    : guictr_base(), grid(_grid), view(_view), waveformRef(new gui_waveform_texture_ref{}) {
    padding = 0;
}
gui_audiocontent::~gui_audiocontent() {
    delete waveformRef;
}
void gui_audiocontent::renderAudioClip(NVGcontext* vg) {

    nvgSave(vg);
    nvgTranslate(vg, pos.x, pos.y);
    if (waveformRef->rendered) {
        dawCtrl->getWaveformRenderer()->draw(vg, waveformRef, size);
    }

    nvgRestore(vg);
}
void gui_audiocontent::render(NVGcontext* vg) {
    renderAudioClip(vg);
}
void gui_audiocontent::releaseRendered() {
    dawCtrl->getWaveformRenderer()->release(waveformRef);
    waveformRef->rendered = false;
}

audioclip_texture_t makeWaveformFromSample(const int32_t tempo100, const samplerate_t samplerate, scaled_grid& grid, const clip_audio_t& clipAudio,
                                           const ivec2& pos, const ivec2& size) {
    double sampleStartOffset = tickToSampleConvert<double, roundmode::floor>(grid.screenToTickD(0), tempo100, samplerate);
    double sampleEnd         = tickToSampleConvert<double, roundmode::floor>(grid.screenToTickD(size.x), tempo100, samplerate);
    double lenSamples   = sampleEnd - sampleStartOffset;
    double samplesPerPx = lenSamples / size.x;

    audioclip_texture_t w;
    w.quality = 2;

    double pxPerSample      = 1.0 / samplesPerPx;
    constexpr double MAX_RES = 2048.0;

    w.scaleX = 1.0f;
    w.pos    = pos;
    w.size   = ivec2(math::min(size.x, FBO_WIDTH), math::min(size.y, FBO_HEIGHT));

    double nSamplesD = sampleEnd - sampleStartOffset;
    if (nSamplesD * pxPerSample > FBO_WIDTH) {
        samplesPerPx = nSamplesD / double(FBO_WIDTH);
    }
    if (samplesPerPx > MAX_RES && (nSamplesD / double(MAX_RES)) <= FBO_WIDTH) {
        w.scaleX     = float(double(MAX_RES) / samplesPerPx);
        samplesPerPx = MAX_RES;
    }

    dbgassert(w.size.x <= FBO_WIDTH && w.size.y <= FBO_HEIGHT);
    dbgassert(w.size.x > 0);
    w.sampleBegin       = 0;
    w.sampleBeginOffset = math::floordS64(sampleStartOffset);
    w.sampleEnd         = math::floordS64(sampleEnd);
    w.samplesPerPx      = samplesPerPx;
    w.linewidth         = 3.0f;
    //if (samplesPerPx >= 8.0)
    //    w.method = SampleMethod::sample_energy;
    //else
    w.method  = SampleMethod::sample_straight;
    w.audioId = clipAudio.id;
    w.clipped = true;
    //log_lf(Log::L_DEBUG, "waveform %d - %d - %d - %d %f %f %f\n", w.audioId, w.sampleBegin, w.sampleBeginOffset, w.sampleEnd, w.samplesPerPx, grid.zoom, lenSamples);
    //log_lf(Log::L_DEBUG, "waveform[height:%d,zoom:%f,q:%d,w:%f,smp/px:%f,scale:%f]\n", w.size.y, grid.zoom, w.quality, w.linewidth, w.samplesPerPx, w.scaleX);


    return w;
}
inline bool isAlmostEqualWaveformSample(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) {
    if ((lhs.sampleBeginOffset - lhs.sampleBegin) == (rhs.sampleBeginOffset - rhs.sampleBegin) &&
        (lhs.sampleEnd - lhs.sampleBegin) == (rhs.sampleEnd - rhs.sampleBegin) &&
        //lhs.startOffset == rhs.startOffset &&
        //lhs.size == rhs.size &&
        //lhs.samplesPerPx == rhs.samplesPerPx &&
        //lhs.scale == rhs.scale &&
        lhs.scaleX == rhs.scaleX &&
        lhs.audioId == rhs.audioId &&
        lhs.sampleVersion == rhs.sampleVersion &&
        lhs.quality == rhs.quality && lhs.method == rhs.method) {
        if (lhs.fades != rhs.fades)
            return false;
        if (lhs.loopPos != rhs.loopPos) {
            return false;
        }
        if (lhs.clipped || rhs.clipped)
            return lhs.scaleX == rhs.scaleX && lhs.scaleY == rhs.scaleY && lhs.size == rhs.size && lhs.samplesPerPx == rhs.samplesPerPx;
        vec2 sd    = vec2(math::absvec2(lhs.size - rhs.size));
        vec2 limit = vec2(lhs.size) / 4.0f;
        return sd.x < limit.x && sd.y < limit.y;
    }
    return false;
}
void gui_audiocontent::updatePosition() {
    const clip_t* clip         = view.clip();
    if (!clip || clip->clipType != CLIP_AUDIO) {
        releaseRendered();
        return;
    }
    auto& clipAudio    = clip->audio;
    audiofile_t* audio = dawCtrl->getDaw()->getAudioCache()->get(clipAudio.id);
    if (!audio) {
        releaseRendered();
        return;
    }
    const auto tempo100 = dawCtrl->getDaw()->getGlobals().tempo100;
    const auto samplerate = view.track()->audio->sampleFormat.sampleRate;

    dbgassert(size.x > 0);
    audioclip_texture_t waveform = makeWaveformFromSample(tempo100, samplerate, grid, clipAudio, ivec2(0, 0), size);
    if (waveform.size.x < 1 || waveform.size.y < 1) {
        releaseRendered();
        waveformRef->waveform = waveform;
        this->updatedWaveform = waveform;
        return;
    }

    if (dawCtrl->getWaveformRenderer()->canQueueUpdate()) {
        bool equal = waveform.size == waveformRef->waveform.size &&
                     clipAudio.id == waveformRef->waveform.audioId &&
                     isAlmostEqualWaveformSample(waveform, waveformRef->waveform);
        if (!equal) {
            this->updatedWaveform = waveform;
        }
    }
}
void gui_audiocontent::onTick(AppCtrl* appctrl) {
    if (tickOffset++ > 60) {
        tickOffset = 0;
        //updatePosition();
    }
}
void gui_audiocontent::prerender(NVGcontext* vg) {
    clip_t* clip = view.clip();
    if (!clip || clip->clipType != CLIP_AUDIO) {
        return;
    }
    auto& clipAudio    = clip->audio;
    audiofile_t* audio = dawCtrl->getDaw()->getAudioCache()->get(clipAudio.id);
    if (!waveformRef->queued) {
        if (!audio || this->updatedWaveform.size.x < 1 || this->updatedWaveform.size.y < 1) {
            return;
        }
        if ((!waveformRef->rendered || (this->updatedWaveform != waveformRef->waveform))) {
            //releaseRendered();
            dbgassert(!waveformRef->queued);
            waveformRef->waveform = this->updatedWaveform;
            dbgassert(waveformRef->waveform.size.x > 0 && waveformRef->waveform.size.y > 0);
            if (dawCtrl->getWaveformRenderer()->queueUpdate(audio, waveformRef)) {
                dbgassert(waveformRef->queued);
            }
        }
    }
}

void gui_audiocontent::layout() {
    for (guibase* gui: guis) {
        gui->layout();
    }
}

bool gui_audiocontent::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    return false;
}

guictr_audioeditor::guictr_audioeditor(guictr_clipeditor& parentClipEditor, clip_view& _view)
    : guictr_editor_base(parentClipEditor, _view),
      content(grid, _view),
      clipHandles(*this, grid) {
    padding = 2;
    grid.showRange(0, TICKS_BAR * 4);
    grid.addCallback(this);
    add(&content);
    add(&timeline);
    add(&clipHandles);
}

guictr_audioeditor::~guictr_audioeditor() {
    remove(&timeline);
    remove(&content);
    remove(&clipHandles);
}

void guictr_audioeditor::buttonClicked(guibase* button) {
}

void guictr_audioeditor::renderBackground(NVGcontext* vg) {
    drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
}

int32_t guictr_audioeditor::getTotalWidth() {
    return math::max(10000, getSizeContent().x);
}

void guictr_audioeditor::layout() {
    ivec2 cs         = getSizeContent();
    timeline.pos     = ivec2(0, 0);
    timeline.size    = ivec2(cs.x, heightTimeLine);
    clipHandles.pos  = ivec2(timeline.left(), timeline.bottom()+ heightSelIndicator);
    clipHandles.size = ivec2(timeline.size.x, heightLoopInidicator * 2);
    content.pos      = ivec2(timeline.left(), clipHandles.bottom());
    content.size     = ivec2(timeline.size.x, cs.y - heightTimeLine - heightClipIndicators);

    grid.update(content.size);
    for (guibase* gui: guis) {
        gui->layout();
    }
}

void guictr_audioeditor::gridChanged(scaled_grid& _grid) {
    ivec2 cs = getSizeContent();
    _grid.update(ivec2(timeline.size.x, cs.y - 30));
    content.updatePosition();
}

void guictr_audioeditor::handleDraggedBegin(MouseEvent& evt) {
    //if (evt.guiDragged == &piano) {
    //if (evt.type == M_EVT_DOUBLECLICK)
    //zoomPianoRollToClipsNoteRange();
    //
    //return;
    //}
}

void guictr_audioeditor::showEditClip() {
    this->clipHandles.getClipView() = view;
    clip_t* clip = view.clip();
    if (clip) {
        auto& layout = clip->editorLayout;
        if (layout.noLayout) {
            grid.showRange(clip->offsetStart, clip->offsetStart + clip->getLen());
        } else {
            grid.setLayout(layout.layoutGrid);
        }
    }
    content.updatePosition();
}

void guictr_audioeditor::storeLayout() {
    clip_t* clip = view.clip();
    if (clip) {
        auto& layout = clip->editorLayout;
        layout.layoutGrid = grid;//TODO: add a cast to get rid of slicing warning
        layout.noLayout   = false;
    }
}

bool guictr_audioeditor::handleKeyInput(KeyEvent& kevt) {
    return content.handleKeyInput(kevt);
}
bool guictr_audioeditor::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    if (isCtrl(evt.kbmods)) {
        float zomDelta   = 1.0f + yoffset * -0.2f;
        ivec2 localMouse = timeline.toContainerSpace(evt.relMousepos);
        timeline.adjustZoom(localMouse.x, zomDelta);
    } else if (isShift(evt.kbmods)) {
        timeline.adjustOffset(-yoffset * 32);
    } else {
        //piano.setOffset(offset() + yoffset * 2.0* scale());
    }
    return true;
}

void guictr_audioeditor::render(NVGcontext* vg) {

    renderBackground(vg);
    if (!setScissorTransform(vg)) {
        return;
    }
    nvgSave(vg);
    timeline.render(vg);
    nvgRestore(vg);
    nvgSave(vg);
    content.render(vg);
    nvgRestore(vg);
    auto playbackPos = dawCtrl->getDaw()->getPlaybackPos();
    auto clip = view.clip();
    auto handlesPos = timeline.getLeftBottom();
    auto handlesSize = vec2(clipHandles.size) + vec2(0, heightSelIndicator);
    nvgSave(vg);
    nvgIntersectScissor(vg, handlesPos.x, handlesPos.y, handlesSize.x, handlesSize.y);
    renderClipHandlesBackground(vg, theme, grid, clipHandles.pos, clipHandles.size);
    renderGridList(vg, theme, grid, handlesPos, handlesSize);
    nvgSave(vg);
    clipHandles.render(vg);
    nvgRestore(vg);
    clipHandles.renderLoopHandle(vg, {handlesSize.x, content.bottom() - handlesPos.y});
    renderSelectionIndicator(vg, theme, grid, handlesPos, vec2(timeline.size.x, heightSelIndicator), clip, dawCtrl->getCursor(), heightSelIndicator);
    nvgRestore(vg);
    renderPlayHead(vg, theme, grid, pos, size, clip, playbackPos, view.isAbsoluteTimeMode(), 1.0f);
}
