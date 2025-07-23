#include <algorithm>
#include <cstdio>
#include <memory>
#include <nanovg.h>
#include <nanovg_min.h>
#include "appsettings.hpp"
#include "assert_dbg.h"
#include "clipeditor.hpp"

#include "color_util.hpp"
#include "event.hpp"
#include "gui/container/container.hpp"
#include "gui/track/trackctr.hpp"
#include "guiconstant.hpp"
#include "host/audiosample.hpp"
#include "host/clip/clip.hpp"
#include "math/seq_math.hpp"
#include "gui/gui.hpp"
#include "guicolors.hpp"
#include "plugins/synth/IPlugMidi.hpp"
#include "seq_util.hpp"
#include "str_util.hpp"
#include "host/track/track.hpp"
#include "host/track/track_impl.hpp"
#include "note.hpp"
#include "seq_time.hpp"
#include "cursor.hpp"
#include "keyboard.hpp"
#include "grid.hpp"
#include "tls.hpp"
#include "types.hpp"
#include "wave/waveform_render.hpp"
#include "wave/waveform_render_impl.hpp"

#include "gui/contextmenu/contextmenu_daw.hpp"
#include "logging.hpp"


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
        if (!parentEditor.getClipView().isAbsoluteTimeMode())
            return;
        if (evt.type == M_EVT_DOUBLECLICK) {
            // zoom to clip
            parentEditor.getGrid().showRange(clip->start(), clip->end());
            parentEditor.getClipEditor().getNoteEditor().zoomPianoRollToClipsNoteRange(clip);
        } else {
            parentEditor.getClipEditor().selectEditClip(view.clip());
        }
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
    trackdata_clips_t& midi = track->getClips();
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
    bool bIsAbsoluteTimeMode = view.isAbsoluteTimeMode();
    if (bIsAbsoluteTimeMode) {
        if (clip->offsetStart < clip->loopStart) {
            curLoopEnd = clip->loopStart - clip->offsetStart + clip->loopLen;
        } else {
            curLoopEnd = clip->loopLen;
        }
    }
    if (dragHandle == drag_handle_right) {
        if (!bIsAbsoluteTimeMode) {
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
        if (!bIsAbsoluteTimeMode) {
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
        tick_t newLen    = clip->loopLen + (tickRelative - curLoopEnd);
        if (newLen > 0) {
            if (clip->loopLen == newLen)
                return;
            clip->loopLen = math::max(0, newLen);
        }
    }
    if (dragHandle == drag_handle_loopleft) {
        if (!bIsAbsoluteTimeMode) {
            tick_t curLoopStart = clip->loopStart;
            tick_t tickDelta    = (tickRelative - curLoopStart);
            tick_t newStart     = clip->loopStart + tickDelta;
            if (newStart < curLoopEnd) {
                if (clip->loopStart == newStart && clip->loopLen == curLoopEnd - newStart)
                    return;
                clip->loopStart = math::max(0, newStart);
                clip->loopLen   = math::max(0, curLoopEnd - newStart);
            }
        }
    }
    if (dragHandle == drag_handle_loopbar) {
        tick_t curLoopStart = clip->loopStart;
        tick_t tickDelta    = (tickRelative - curLoopStart);
        if (!tickDelta)
            return;
        bool bMatch = clip->offsetStart == clip->loopStart;
        if (isAlt(evt.kbmods)) {
            clip->loopStart = math::max(0, tickRelative);
        } else {
            clip->loopStart = math::max(0, clip->loopStart + tickDelta);
            if (!bIsAbsoluteTimeMode || bMatch) {
                clip->offsetStart = clip->loopStart;
            }
        }
    }
    clip->setDirty();
    dawCtrl->getDaw()->updateVisibleTrackContents();
}

void guictr_cliphandles::handleDraggedRelease(MouseEvent& evt) {
    dragHandle = drag_handle_none;
    //TODO: handle history
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
        if (view.isAbsoluteTimeMode()) {
            for (auto& hndl : hndls) {
                if (hndl.mode == dragmode::drag_handle_loopleft) {
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
            // auto absTimeMode = parentEditor.getClipView().isAbsoluteTimeMode();
            if (/* absTimeMode &&  */containsHandlePos(local)) {
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
    float tickBeginX = 0;
    float tickEndX   = 0;

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

        if (!view.isAbsoluteTimeMode() && tickBeginX > -wLoopInidicator && tickBeginX < cs.x + wLoopInidicator) {
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
        NVGcolor color  = rgbToNvg(clip->rgb);
        nvgBeginPath(vg);
        nvgRect(vg, barBeginX, yOffset, barEndX - barBeginX, heightLoopInidicator * 2);
        nvgFillColor(vg, color);
        nvgFillCustomPar(vg, -4);
        nvgFill(vg);
        nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CLIP_OUTLINE));
        nvgStrokeWidth(vg, 1.f);
        nvgStroke(vg);
        if (clip->loopEnabled && clip->loopLen > 0) {
            tick_t firstLoop = 0;
            if (clip->offsetStart < clip->loopStart) {
                firstLoop = clip->loopStart - clip->offsetStart;
            }
            nvgBeginPath(vg);
            int n = 0;
            for (tick_t posLoopIndicator = firstLoop; posLoopIndicator < clip->getLen(); posLoopIndicator += clip->loopLen) {
                if (posLoopIndicator >= 0) {
                    auto x = grid.tickToScreenD(posLoopIndicator + clip->time);
                    if (x < -5) {
                        continue;
                    }
                    if (x > size.x + 5) {
                        continue;
                    }
                    n++;
                    nvgMoveTo(vg, x, 0);
                    nvgLineTo(vg, x, 0 + float(size.y) / 4);
                    nvgMoveTo(vg, x, 0 + float(size.y) * 3 / 4);
                    nvgLineTo(vg, x, 0 + float(size.y));
                }
            }
            if (n != 0) {
                nvgStrokeColor(vg, theme->getFrameColorBase());
                nvgStrokeWidth(vg, 1.f);
                nvgStroke(vg);
            }
        }
        if (clip->name.length() && barEndX - barBeginX > 12 && heightLoopInidicator * 2 > 12) {
            renderTextLabel(vg,
                            {barBeginX + heightLoopInidicator/2, yOffset + heightLoopInidicator},
                            {barEndX - barBeginX - heightLoopInidicator, heightLoopInidicator * 2},
                            clip->name,
                            theme,
                            heightLoopInidicator * 2,
                            getContrastFontColor(clip->rgb),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
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
            tickPos = viewClip->getLoopedTick(playbackPos - viewClip->start());
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
    if (!isRenderableSizeAndContext(vg))
        return;
    nvgIntersectScissor(vg, pos.x, pos.y, size.x, size.y);
    nvgTranslate(vg, pos.x, pos.y);
    renderHandle(vg, 0);
}

guictr_noteeditor::guictr_noteeditor(guictr_clipeditor& parentClipEditor, clip_view_t& _view)
    : guictr_editor_base(parentClipEditor, &content, _view), layout_pianoroll_t(),
      piano(_view, *this),
      content(m_grid, _view, *this),
      velocities(m_grid, _view, *this),
      ctrlData(m_grid, _view, *this),
      dropdownSelectControlData(&ctrlData),
      splitterVel(0, 0.75f)
{
    padding = 2;
    splitterVel.setMinMax(0.1f, 0.9f);
    splitterVel.setCallback(this);
    m_grid.setNegativeGrid(true);
    m_grid.showRange(0, TICKS_BAR * 4);
    m_grid.addCallback(this);
    add(&piano);
    add(&content);
    add(&ctrlData);
    add(&velocities);
    add(&timeline);
    add(&dropdownSelectControlData);
    add(&splitterVel);
    btnToggleFold.setStateRef(&bFoldNotes);
    btnShowVelocities.setStateRef(&bShowVelocity);
    btnShowControlData.setStateRef(&bShowControlData);
    content.showRange(CLIPEDITOR_DEFAULT_MIN, CLIPEDITOR_DEFAULT_MAX);
    btnToggleFold.setButtonColor(GuiColor::COL_FOLD_BUTTON);
    btnShowArp.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
    btnShowClipSettings.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
    btnShowControlData.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
    btnShowVelocities.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
    btnShowClipSettings.setText("Clip");
    btnShowArp.setText("Arp");
    btnToggleFold.setText("Fold");
    btnShowVelocities.setText("Velocity");
    btnShowControlData.setText("CC");
    dropdownSelectControlData.setText("Select Channel");
    btnShowArp.setTooltipText("Show Arpeggiator");
    btnShowClipSettings.setTooltipText("Show Clip Settings");
    btnShowControlData.setTooltipText("Show Control Data");
    btnShowVelocities.setTooltipText("Show Velocity");
    btnToggleFold.setTooltipText("Toggle Fold");
    
    for (auto* btn : buttonList) {
        add(btn);
    }
}

void guictr_noteeditor::setControl(BaseCtrl *parentCtrl) {
    guictr_editor_base::setControl(parentCtrl);
    auto& dawSettings = daw_tls::getDawSettings();
    btnShowArp.setStateRef(&dawSettings.uiShowSettingsArp);
    btnShowClipSettings.setStateRef(&dawSettings.uiShowSettingsClip);
}

guictr_noteeditor::~guictr_noteeditor() {
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
    if (button == &timeline) {
        auto currentClip = view.clip();
        if (!currentClip)
            return;
        if (view.isAbsoluteTimeMode()) {
            m_grid.showRange(view.m_selectionView.viewBegin, view.m_selectionView.viewEnd);
        } else {
            m_grid.showRange(currentClip->notes.firstNote.start(), math::max<tick_t>(currentClip->notes.lastNote.end(), currentClip->loopStart + currentClip->loopLen));
        }
        return;
    }
    if (button == &btnToggleFold) {
        bFoldNotes = !bFoldNotes;
        view.updateNotePitches(true);
        if (bFoldNotes && yscalefold == 0 && yoffsetfold == 0) {
            zoomPianoRollToClipsNoteRange(nullptr);
        }
    }
    if (button == &btnShowVelocities) {
        bool isShown = bShowVelocity && this->velocities.isVisible();
        this->velocities.setVisible(!isShown);
        bShowVelocity = !isShown;
        parent->layout();
    }
    if (button == &btnShowControlData) {
        bool isShown = bShowControlData && this->ctrlData.isVisible();
        this->ctrlData.setVisible(!isShown);
        bShowControlData = !isShown;
        parent->layout();
    }
    if (button == &btnShowClipSettings) {
        auto& dawSettings = daw_tls::getDawSettings();
        dawSettings.uiShowSettingsClip = !dawSettings.uiShowSettingsClip;
        parent->layout();
    }
    if (button == &btnShowArp) {
        auto& dawSettings = daw_tls::getDawSettings();
        dawSettings.uiShowSettingsArp = !dawSettings.uiShowSettingsArp;
        parent->layout();
    }
}

void guictr_noteeditor::renderBackground(NVGcontext* vg) {
    drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
}


void guictr_noteeditor::gridChanged(scaled_grid& _grid) {
    _grid.update(sizeContentArea);
}

void guictr_noteeditor::handleDraggedBegin(MouseEvent& evt) {
    if (evt.guiDragged == &piano) {
        if (evt.type == M_EVT_DOUBLECLICK)
            zoomPianoRollToClipsNoteRange(nullptr);
        return;
    }
    if (evt.guiDragged == this && evt.type == M_EVT_BTN_DOWN
        && evt.relMousepos.y > timeline.bottom() 
        && evt.relMousepos.y < timeline.bottom() + handlesHeight 
        && view.isAbsoluteTimeMode()) {
        parentClipEditor.selectEditClip(nullptr);
    }
}

void guictr_noteeditor::zoomPianoRollToClipsNoteRange(clip_t* optionalClipOnly) {
    int32_t minSemi = -1;
    int32_t maxSemi = -1;
    if (bFoldNotes) {
        minSemi = 0;
        maxSemi = view.notePitches.size();
    } else {
        view.visitClipView([&](clip_t* clip) {
            if (optionalClipOnly && clip != optionalClipOnly)
                return true;
            if (clip->notes.isEmpty())
                return true;
            if (view.isAbsoluteTimeMode()) {
                auto& view = clip->getNoteViewRender();
                minSemi = minSemi == -1 ? view.minNote.pitch : math::min<int32_t>(minSemi, view.minNote.pitch);
                maxSemi = maxSemi == -1 ? view.maxNote.pitch : math::max<int32_t>(maxSemi, view.maxNote.pitch);
            } else {
                minSemi = minSemi == -1 ? clip->notes.minNote.pitch : math::min<int32_t>(minSemi, clip->notes.minNote.pitch);
                maxSemi = maxSemi == -1 ? clip->notes.maxNote.pitch : math::max<int32_t>(maxSemi, clip->notes.maxNote.pitch);
            }
            return true;
        });
    }
    if (minSemi == -1 || maxSemi == -1) {
        return;
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

void guictr_noteeditor::selectEditClip(clip_t* clip) {
    guictr_editor_base::selectEditClip(clip);
    updateCopiedClipData();
}
void guictr_noteeditor::updateCopiedClipData() {
    ctrlData.showEditClip();
}
void guictr_noteeditor::relayout() {
    guictr_editor_base::relayout();
    updateCopiedClipData();
    clip_t* currentClip = view.clip();
    bool bIsAbsMode = view.isAbsoluteTimeMode();
    clip_editor_layout_t layout = lastLayout;
    if (!bIsAbsMode && currentClip) {
        if (!currentClip->editorLayout.noLayout) {
            layout = currentClip->editorLayout;
        } else {
            layout.noLayout = true;
        }
    } else if (bIsAbsMode && view.m_selectionView.totalClipCount) {
        if (!view.m_selectionView.editorLayout.noLayout) {
            layout = view.m_selectionView.editorLayout;
        } else {
            layout.noLayout = true;
        }
        layout = view.m_selectionView.editorLayout;
    }

    if (!layout.noLayout) {
        setLayout(layout.layoutPianoRoll);
    } else {
        layout_pianoroll_t defaultLayout{};
        setLayout(defaultLayout);
    }

    zoomPianoRollToClipsNoteRange(nullptr);
}

void guictr_editor_base::relayout() {
    clip_t* currentClip = view.clip();
    bool bIsAbsMode = view.isAbsoluteTimeMode();
    clip_editor_layout_t layout = lastLayout;
    if (!bIsAbsMode && currentClip) {
        if (!currentClip->editorLayout.noLayout) {
            layout = currentClip->editorLayout;
        } else {
            layout.noLayout = true;
        }
        m_grid.setLayout(layout.layoutGrid);
    } else if (bIsAbsMode && !view.m_selectionView.editorLayout.noLayout && view.m_selectionView.totalClipCount) {
        layout = view.m_selectionView.editorLayout;
        m_grid.setLayout(layout.layoutGrid);
    }
    lastLayout = layout;
    auto newClipHandleCount = bIsAbsMode  ? view.m_selectionView.totalClipCount : 1;
    auto curClipHandleCount = clipsHandles.size();
    for (size_t i = newClipHandleCount; i < curClipHandleCount; i++) {
        clipsHandles[i]->setVisible(false);
        clipsHandles[i]->getClipView().reset();
    }
    for (size_t i = curClipHandleCount; i < newClipHandleCount; i++) {
        if (clipsHandles.size() <= i || !clipsHandles[i]) {
            clipsHandles.push_back(std::make_shared<guictr_cliphandles>(*this, m_grid));
            this->add(clipsHandles[i].get());
        }
    }
    auto numTracks = view.m_selectionView.tracks.size();
    if (!bIsAbsMode) {
        numTracks = 1;
    }
    handlesHeight = math::max<int32_t>(1, numTracks) * heightLoopInidicator * 2 + heightSelIndicator;
    auto it = clipsHandles.begin();
    if (!bIsAbsMode) {
        numTracks = 1;
        auto& clipHandles = **(it++);
        clipHandles.setVisible(true);
        clipHandles.getClipView().setSingleClip(currentClip);
        clipHandles.getClipView().bIsAbsoluteMode = bIsAbsMode;
        clipHandles.setTrackSelectionIdx(0);
        dbgassert(clipHandles.getClipView().clip() == currentClip);
        clipHandles.setHandleActive(true);
        moveToBegin(&clipHandles);
        return;
    } else {
    }
    for (size_t trackIdx = 0; trackIdx < numTracks; trackIdx++) {
        auto& [trackEntry, vecTrackClips] = view.m_selectionView.tracks[trackIdx];
        if (!view.clipRef().isTrackValid(trackEntry.track)) {
            continue;
        }
        auto numClipsOnTrack = vecTrackClips.size();
        for (size_t clipIdx = 0; clipIdx < numClipsOnTrack && it != clipsHandles.end(); clipIdx++) {
            auto& selClip = vecTrackClips[clipIdx];
            if (!trackEntry.track->getClips().hasClip(selClip)) {
                continue;
            }
            auto& clipHandles = **(it++);
            dbgassert(it <= clipsHandles.end());
            clipHandles.setVisible(true);
            clipHandles.getClipView().setSingleClip(selClip);
            clipHandles.getClipView().bIsAbsoluteMode = bIsAbsMode;
            clipHandles.setTrackSelectionIdx(trackIdx);
            dbgassert(clipHandles.getClipView().clip() == selClip);
            clipHandles.setHandleActive(selClip == currentClip);
            moveToBegin(&clipHandles);
        }
    }
}

void guictr_editor_base::onClipChanged() {
    auto currentClip = view.clip();
    bool bIsAbsMode = view.isAbsoluteTimeMode();
    //TODO: this should only be done when the clip or m_selectionView changes
    if (bIsAbsMode) {
        if (view.m_selectionView.totalClipCount) {
            getGrid().showRange(view.m_selectionView.viewBegin, view.m_selectionView.viewEnd);
            storeEditorLayout();
        }
    } else if (currentClip) {
        if (currentClip->editorLayout.noLayout) {
            // getGrid().showRange(currentClip->notes.firstNote.start(), math::max<tick_t>(currentClip->notes.lastNote.end(), currentClip->loopStart + currentClip->loopLen));
            // storeEditorLayout();
        }
    }
}

void guictr_editor_base::selectEditClip(clip_t* clip) {
    if (!assert_expr(clipsHandles.size() >= view.m_selectionView.totalClipCount)) {
        return;
    }
    clip_t* currentClip = view.clip();
    auto numTracks = view.m_selectionView.tracks.size();
    auto it = clipsHandles.begin();
    for (size_t trackIdx = 0; trackIdx < numTracks; trackIdx++) {
        auto& [trackEntry, vecTrackClips] = view.m_selectionView.tracks[trackIdx];
        if (!view.clipRef().isTrackValid(trackEntry.track)) {
            continue;
        }
        auto numClipsOnTrack = vecTrackClips.size();
        for (size_t clipIdx = 0; clipIdx < numClipsOnTrack && it != clipsHandles.end(); clipIdx++) {
            auto& selClip = vecTrackClips[clipIdx];
            if (!trackEntry.track->getClips().hasClip(selClip)) {
                continue;
            }
            auto& clipHandles = **(it++);
            dbgassert(it <= clipsHandles.end());
            clipHandles.setHandleActive(selClip == currentClip);
            if (clipHandles.isHandleActive()) {
                moveToBegin(&clipHandles);
            }
        }
    }
}

void guictr_editor_base::storeEditorLayout() {
    clip_t* currentClip = view.clip();
    bool bIsAbsMode = view.isAbsoluteTimeMode();
    clip_editor_layout_t* layoutDst = nullptr;
    if (bIsAbsMode && view.m_selectionView.totalClipCount) {
        layoutDst = &view.m_selectionView.editorLayout;
    }
    if (!bIsAbsMode && currentClip) {
        layoutDst = &currentClip->editorLayout;
    }
    if (!layoutDst) {
        layoutDst = &lastLayout;
    }
    layoutDst->layoutGrid = getGrid();
}

void guictr_noteeditor::storeEditorLayout() {
    clip_t* currentClip = view.clip();
    if (currentClip && currentClip->clipType != CLIP_MIDI) {
        return;
    }
    bool bIsAbsMode = view.isAbsoluteTimeMode();
    clip_editor_layout_t* layoutDst = nullptr;
    if (bIsAbsMode && view.m_selectionView.totalClipCount) {
        layoutDst = &view.m_selectionView.editorLayout;
        layoutDst->noLayout = false;
    }
    if (!bIsAbsMode && currentClip) {
        layoutDst = &currentClip->editorLayout;
        layoutDst->noLayout = false;
    }
    if (!layoutDst) {
        layoutDst = &lastLayout;
    }
    layoutDst->layoutGrid = getGrid();
    layoutDst->layoutPianoRoll = *static_cast<layout_pianoroll_t*>(this);
}

bool guictr_noteeditor::handleKeyInput(KeyEvent& kevt) {
    return content.handleKeyInput(kevt);
}

bool guictr_noteeditor::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    if (isCtrl(evt.kbmods)) {
        if (isShift(evt.kbmods)) {
            ivec2 localMouse = piano.toContainerSpace(evt.relMousepos);
            auto evt2 = evt;
            evt2.relMousepos = localMouse;
            piano.handleMouseScroll(evt2, xoffset, yoffset);
        } else {
            float zomDelta   = 1.0f + yoffset * -0.2f;
            ivec2 localMouse = timeline.toContainerSpace(evt.relMousepos);
            timeline.adjustZoom(localMouse.x, zomDelta);
        }
    } else if (isShift(evt.kbmods)) {
        timeline.adjustOffset(-yoffset * 32);
    } else {
        piano.setOffset(offset() + yoffset * 2.0 * scale());
    }
    content.handleMouseScroll(evt, xoffset, yoffset);
    return true;
}

void guictr_noteeditor::setLayout(layout_pianoroll_t& layout) {
    yscale      = layout.yscale;
    yoffset     = layout.yoffset;
    yscalefold  = layout.yscalefold;
    yoffsetfold = layout.yoffsetfold;
    bFoldNotes  = layout.bFoldNotes;
}

void renderClipHandlesBackground(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid, vec2 handlesPos, vec2 handlesSize) {
    nvgBeginPath(vg);
    nvgRect(vg, handlesPos.x - 2, handlesPos.y, handlesSize.x + 2, handlesSize.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
    nvgFill(vg);
}
void renderGridList(NVGcontext* vg, const guitheme_t* theme, const scaled_grid& grid, vec2 handlesPos, vec2 handlesSize) {
    auto& gridList = grid.getActiveGrid();
    for (auto& g : gridList) {
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
void guictr_noteeditor::renderClipHandles(NVGcontext* vg) {
    guictr_editor_base::renderClipHandles(vg);
}
void guictr_editor_base::renderClipHandles(NVGcontext* vg) {
    auto playbackPos = dawCtrl->getDaw()->getPlaybackPos();
    auto clip = view.clip();
    auto handlesPos = timeline.getLeftBottom();
    auto handlesSize = ivec2(timeline.size.x, handlesHeight);
    if (handlesSize.x > 5 && handlesSize.y > 5) {
        nvgSave(vg);
        nvgIntersectScissor(vg, handlesPos.x, handlesPos.y, handlesSize.x, handlesSize.y);
        renderClipHandlesBackground(vg, theme, m_grid, vec2{0, heightSelIndicator}+vec2(handlesPos), handlesSize);
        int32_t trackIdx = 0;
        int32_t activeTrackIdx = -1;
        auto viewTrack = view.track();
        auto trackHandleHeight = (handlesHeight-heightSelIndicator) / math::max(1, CtrSize(view.m_selectionView.tracks));
        for (auto& [trackEntry, vecClips] : view.m_selectionView.tracks) {
            if (!view.clipRef().isTrackValid(trackEntry.track)) {
                continue;
            }
                // nvgRect(vg, handlesPos.x, handlesPos.y + heightSelIndicator + trackHandleHeight * trackIdx+trackHandleHeight-heightSelIndicator, timeline.size.x, heightSelIndicator);
            auto col = trackIdx % 2 == 0 ? GuiColor::COL_GRID_DRK : GuiColor::COL_GRID_BRT;
            auto nvgCol = viewTrack == trackEntry.track ? rgbToNvg(trackEntry.track->rgb) : theme->getColor(col);
            nvgBeginPath(vg);
            nvgRect(vg, handlesPos.x, handlesPos.y + heightSelIndicator + trackHandleHeight * trackIdx + 2, handlesSize.x, trackHandleHeight-2);
            nvgCol.a *= 0.5f;
            nvgFillColor(vg, nvgCol);
            nvgFillCustomPar(vg, -1);
            nvgFill(vg);
            if (viewTrack == trackEntry.track) {
                activeTrackIdx = trackIdx;
            }
            trackIdx++;
        }
        renderGridList(vg, theme, m_grid, handlesPos, handlesSize);
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
                } else if (clip) {
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
        renderSelectionIndicator(vg, theme, m_grid, handlesPos, vec2(timeline.size.x, heightSelIndicator), view.isAbsoluteTimeMode()?nullptr:clip, dawCtrl->getCursor(), heightSelIndicator);
        // renderPlayHead(vg, theme, grid, handlesPos, handlesSize, clip, playbackPos, view.isAbsoluteTimeMode(), 1.0f);
        nvgRestore(vg);
        if (viewClipHandle) {
            nvgTranslate(vg, viewClipHandle->pos.x, viewClipHandle->pos.y);
            viewClipHandle->renderLoopHandle(vg, vec2(viewClipHandle->size.x, pContent->bottom() - viewClipHandle->top()));
            nvgTranslate(vg, -viewClipHandle->pos.x, -viewClipHandle->pos.y);
        }
        renderPlayHead(vg, theme, m_grid, pContent->pos, pContent->size, clip, playbackPos, view.isAbsoluteTimeMode(), 0.75f);
    }
}
void guictr_noteeditor::render(NVGcontext* vg) {
    renderBackground(vg);
    if (!setScissorTransform(vg)) {
        return;
    }
#ifndef NDEBUG
    for (auto& c : clipsHandles) {
        dbgassert(c->pos.x == timeline.pos.x && c->pos.x == pianoWidth);
    }
#endif
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
    renderClipHandles(vg);
    for (auto* btn : buttonList) {
        if (btn->isVisible()) {
            nvgSave(vg);
            btn->render(vg);
            nvgRestore(vg);
        }
    }
    if (dropdownSelectControlData.isVisible())
        dropdownSelectControlData.render(vg);
    if (splitterVel.isVisible())
        splitterVel.render(vg);
}


gui_audiocontent::gui_audiocontent(scaled_grid& _grid, clip_view_t& _view)
    : gui_clipcontent_base(_grid, _view), waveformRef(new gui_waveform_texture_ref{}) {
    padding = 0;
}
gui_audiocontent::~gui_audiocontent() {
    delete waveformRef;
}
void gui_audiocontent::renderAudioClip(NVGcontext* vg) {

    nvgSave(vg);
    nvgTranslate(vg, pos.x, pos.y);
    if (waveformRef->rendered) {
        auto colWaveform = GuiColor::COL_WAVEFORM;
        auto clip = view.clip();
        if (clip && !clip->enabled) {
            colWaveform = GuiColor::COL_WAVEFORM_MUTED;
        }
        auto rgba = theme->getColor(colWaveform);
        dawCtrl->getWaveformRenderer()->draw(vg, waveformRef, size, rgba);
    }

    nvgRestore(vg);
}
void gui_audiocontent::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
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
    w.audioId = clipAudio.idDerived > -1 ? clipAudio.idDerived : clipAudio.id;
    w.samplesClipped = 0;
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
        if (lhs.samplesClipped || rhs.samplesClipped)
            return lhs.scaleX == rhs.scaleX && lhs.scaleY == rhs.scaleY && lhs.size == rhs.size && lhs.samplesPerPx == rhs.samplesPerPx;
        vec2 sd    = vec2(math::absvec2(lhs.size - rhs.size));
        vec2 limit = vec2(lhs.size) / 4.0f;
        return sd.x < limit.x && sd.y < limit.y;
    }
    return false;
}
void gui_audiocontent::updatePosition() {
    clip_t* clip         = view.clip();
    if (!clip || clip->clipType != CLIP_AUDIO) {
        releaseRendered();
        return;
    }
    auto& clipAudio    = clip->audio;
    audiofile_t* audio = dawCtrl->getDaw()->getAudioCache()->getDerivedSample(clipAudio);
    if (!audio) {
        releaseRendered();
        return;
    }
    const auto tempo100 = dawCtrl->getDaw()->getGlobals().tempo100;
    const auto samplerate = view.track()->audio->sampleFormat.sampleRate;
    audioclip_texture_t waveform;
    if (size.x > 0) {
        waveform = makeWaveformFromSample(tempo100, samplerate, grid, clipAudio, ivec2(0, 0), size);
    } 
    waveform.sampleVersion = audio->getSample()->sampleVersion;
    if (waveform.size.x < 1 || waveform.size.y < 1) {
        releaseRendered();
        waveformRef->waveform = waveform;
        this->updatedWaveform = waveform;
        return;
    }

    if (dawCtrl->getWaveformRenderer()->canQueueUpdate()) {
        bool equal = waveformRef->rendered && waveform.size == waveformRef->waveform.size &&
                     waveform.audioId == waveformRef->waveform.audioId &&
                     isAlmostEqualWaveformSample(waveform, waveformRef->waveform);
        if (!equal) {
            this->updatedWaveform = waveform;
        }
    }
}
void gui_audiocontent::onTick(AppCtrl* appctrl) {
    if (tickTimerRefresh++ > 60) {
        tickTimerRefresh = 0;
        //updatePosition();
    }
}
void gui_audiocontent::prerender(NVGcontext* vg) {
    clip_t* clip = view.clip();
    if (!clip || clip->clipType != CLIP_AUDIO) {
        return;
    }
    auto audio = dawCtrl->getDaw()->getAudioCache()->getDerivedSample(clip->audio);
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
    guictr_base::layout();
}

bool gui_audiocontent::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    auto daw = dawCtrl->getDaw();
    auto command = ctxt.type;
    auto& kevt = ctxt.kevt;
    if (focused() && command == CMD_PASTE && daw->getClipboardType() == ClipBoardType::CLIPBOARD_NOTES) {
        return false;
    }
    clip_t* clip = view.clip();
    if (kevt.type != K_RELEASE) {
        if (kevt.type == K_PRESS) {
            if (command == CMD_REVERSE) {
                if (clip && clip->clipType == CLIP_AUDIO) {
                    clip->audio.settings.flags ^= clip_audio_settings_t::FLAG_REVERSE;
                    dawCtrl->getDaw()->updateDerivedAudio(clip, clip->audio.settings);
                }
                return true;
            }
        }
    }
    static constexpr auto commands = {CMD_REVERSE};
    return std::find(commands.begin(), commands.end(), command) != commands.end();
}

guictr_audioeditor::guictr_audioeditor(guictr_clipeditor& parentClipEditor, clip_view_t& _view)
    : guictr_editor_base(parentClipEditor, &content, _view),
      content(m_grid, _view)
{
    padding = 2;
    m_grid.addCallback(this);
    add(&content);
    add(&timeline);
}

guictr_audioeditor::~guictr_audioeditor() {
    remove(&timeline);
    remove(&content);
}

void guictr_audioeditor::buttonClicked(guibase* button) {
}

void guictr_audioeditor::renderBackground(NVGcontext* vg) {
    drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
}

int32_t guictr_audioeditor::getTotalWidth() {
    return math::max(10000, getSizeContent().x);
}

void guictr_noteeditor::layout() {
    ivec2 cs = getSizeContent();
    pianoWidth = piano.hasNotePitchNames() ? 160 : 100;
    timeline.pos       = ivec2(pianoWidth, 0);
    timeline.size      = ivec2(cs.x - pianoWidth, heightTimeLine);
    guictr_editor_base::layout();
#ifndef NDEBUG
    for (auto& c : clipsHandles) {
        dbgassert(c->pos.x == timeline.pos.x && c->pos.x == pianoWidth);
    }
#endif

    piano.pos  = ivec2(0, timeline.bottom() + handlesHeight);
    piano.size = ivec2(pianoWidth, cs.y - piano.pos.y);
    posContentArea = piano.getRightTop();
    sizeContentArea = cs - posContentArea;
    velHeight = math::clamp(splitterVel.rightOrBottom(sizeContentArea.y), 0, 220);

    if (velocities.isVisible() && ctrlData.isVisible()) {
        velHeight /= 2;
    }

    auto sizeContent = sizeContentArea;
    if (!velocities.isVisible()) {
        velocities.size = ivec2(cs.x - pianoWidth, 0);
    } else {
        sizeContent.y -= velHeight;
        velocities.size = ivec2(cs.x - pianoWidth, velHeight);
    }
    if (!ctrlData.isVisible()) {
        ctrlData.size = ivec2(cs.x - pianoWidth, 0);
    } else {
        sizeContent.y -= velHeight;
        ctrlData.size = ivec2(cs.x - pianoWidth, velHeight);
    }
    piano.size.y       = sizeContent.y;
    content.pos        = posContentArea;
    content.size       = sizeContent;
    velocities.pos     = content.getLeftBottom();
    ctrlData.pos       = velocities.getLeftBottom();
    splitterVel.pos    = piano.getLeftBottom() - Splitter::SPLITTER_LAYOUT_THICKNESS / 2;
    splitterVel.size   = ivec2(cs.x, Splitter::SPLITTER_LAYOUT_THICKNESS);
    auto buttonPos = vec2(padding, padding);
    auto buttonArea = vec2(pianoWidth, timeline.bottom()+handlesHeight);
    auto buttonHeight = theme->get(GuiConstant::CONST_SMALL_LABEL_HEIGHT);
    auto perRow = 2;
    auto buttonWidth = (buttonArea.x - (perRow+1)*padding) / float(perRow);
    for (size_t i = 0; i < buttonList.size(); ++i) {
        auto& button = buttonList[i];
        auto row = i / perRow;
        auto col = i % perRow;
        button->pos = ivec2(buttonPos + vec2(padding + col*(buttonWidth+padding), padding + row*(buttonHeight+padding)));
        button->size = ivec2(buttonWidth, buttonHeight);
    }
    dropdownSelectControlData.setVisible(ctrlData.isVisible());
    if (dropdownSelectControlData.isVisible()) {
        dropdownSelectControlData.pos = ctrlData.getLeftTop() - ivec2(pianoWidth, 0);
        dropdownSelectControlData.size = ivec2(pianoWidth, 18);
    }
    m_grid.update(content.size);
    for (guibase* gui: guis) {
        gui->layout();
    }
}

void guictr_audioeditor::layout() {
    ivec2 cs = getSizeContent();
    timeline.pos       = ivec2(0);
    timeline.size      = ivec2(cs.x, heightTimeLine);
    guictr_editor_base::layout();
    content.pos        = ivec2(timeline.left(), timeline.bottom() + handlesHeight);
    content.size       = ivec2(timeline.size.x, cs.y - (timeline.bottom() + handlesHeight));
    m_grid.update(content.size);
    for (guibase* gui: guis) {
        gui->layout();
    }
}

void guictr_editor_base::layout() {
    auto clipHandlesPos = ivec2(timeline.left(), timeline.bottom() + heightSelIndicator);
    auto insetClipHandleY = 4;
    for (auto& clipHandles : clipsHandles) {
        clipHandles->pos  = clipHandlesPos + ivec2(0, heightLoopInidicator * 2 * clipHandles->getTrackSelectionIdx() + insetClipHandleY);
        clipHandles->size = ivec2(timeline.size.x, heightLoopInidicator * 2 - insetClipHandleY * 2);
    }
}

void guictr_audioeditor::gridChanged(scaled_grid& _grid) {
    ivec2 cs = getSizeContent();
    _grid.update(ivec2(timeline.size.x, cs.y - 30));
    content.updatePosition();
}

void guictr_audioeditor::handleDraggedBegin(MouseEvent& evt) {
    // if (evt.guiDragged == this) {
    //     parentClipEditor.selectEditClip(nullptr);
    // }
}

void guictr_audioeditor::relayout() {
    guictr_editor_base::relayout();
    content.updatePosition();
}

void guictr_audioeditor::storeEditorLayout() {
    auto clip = view.clip();
    if (clip && clip->clipType == CLIP_AUDIO)
        guictr_editor_base::storeEditorLayout();
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
    renderClipHandles(vg);
}
