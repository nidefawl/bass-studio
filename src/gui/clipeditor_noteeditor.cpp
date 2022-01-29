#include <algorithm>
#include "clipeditor.h"

#include "math/seq_math.h"
#include "gui.h"
#include "guicolors.h"
#include "track.h"
#include "track_impl.h"
#include "note.h"
#include "seq_time.h"
#include "cursor.h"
#include "keyboard.h"
#include "grid.h"
#include "wave/waveform_render.h"
#include "wave/waveform_render_impl.h"

#include "guicontextmenu_daw.h"
#include "logging.h"

namespace GuiConstant {
    extern constant_t CONST_CLIPEDITOR_HANDLES_STROKE_WIDTH;
}

void guictr_cliphandles::handleDraggedBegin(MouseEvent& evt) {
    dragHandle   = drag_handle_none;
    clip_t* clip = view.clip();
    if (!clip) {
        return;
    }
    ivec2 local = evt.relMousepos;
    dragHandle  = getDragZone(local);
    dragOffset  = local.x - (int32_t) (grid.tickToScreenD(clip->loopStart));
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
    ThreadLock lock        = MainCtrl::getPlayThread()->lockThread();
    trackdata_midi_t& midi = track->getMidi();
    clip_t* clNext         = midi.getNextClip(clip);
    dbgassert(clNext == NULL || (clNext != clip));
    dbgassert(clNext == NULL || clNext->start() >= clip->end());
    int32_t mousePosX = evt.relMousepos.x;
    if (dragHandle == drag_handle_loopbar) {
        mousePosX -= dragOffset;
    }
    tick_t tickAt     = grid.screenToTickSnap(mousePosX, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
    tick_t curEnd     = clip->offsetStart + clip->getLen();
    tick_t curLoopEnd = clip->loopStart + clip->loopLen;
    if (dragHandle == drag_handle_right) {
        tick_t tickDelta = (tickAt - curEnd);
        tick_t newLen    = clip->getLen() + tickDelta;
        if (newLen > 0) {
            tick_t newEnd = clip->start() + newLen;
            if (clNext && newEnd >= clNext->start()) {
                clip->setLen(clNext->start() - clip->start());
            } else {
                clip->setLen(newLen);
            }
        }
    }
    if (dragHandle == drag_handle_left) {
        tick_t curStart  = clip->offsetStart;
        tick_t tickDelta = (tickAt - curStart);
        tick_t newStart  = clip->offsetStart + tickDelta;
        if (newStart < curEnd) {
            tick_t newLen = curEnd - newStart;
            tick_t newEnd = clip->start() + newLen;
            if (clNext && newEnd >= clNext->start()) {
                clip->setLen(clNext->start() - clip->start());
                clip->offsetStart = curEnd - clip->getLen();
            } else {
                clip->offsetStart = newStart;
                clip->setLen(curEnd - newStart);
            }
        }
    }
    if (dragHandle == drag_handle_loopright) {
        tick_t tickDelta = (tickAt - curLoopEnd);
        tick_t newLen    = clip->loopLen + tickDelta;
        if (newLen > 0) {
            clip->loopLen = newLen;
        }
    }
    if (dragHandle == drag_handle_loopleft) {
        tick_t curLoopStart = clip->loopStart;
        tick_t tickDelta    = (tickAt - curLoopStart);
        tick_t newStart     = clip->loopStart + tickDelta;
        if (newStart < curLoopEnd) {
            clip->loopStart = newStart;
            clip->loopLen   = curLoopEnd - newStart;
        }
    }
    if (dragHandle == drag_handle_loopbar) {
        tick_t curLoopStart = clip->loopStart;
        tick_t tickDelta    = (tickAt - curLoopStart);
        bool inLoop         = clip->offsetStart >= clip->loopStart;
        clip->loopStart += tickDelta;
        if (inLoop && clip->offsetStart < clip->loopStart) {
            clip->offsetStart += clip->loopLen;
        }
        if (inLoop && clip->offsetStart >= clip->loopStart + clip->loopLen) {
            clip->offsetStart -= clip->loopLen;
        }
    }
    clip->setDirty();
    DawInstance::get()->updateVisibleTrackContents();
}

void guictr_cliphandles::handleDraggedRelease(MouseEvent& evt) {
    dragHandle = drag_handle_none;
}

guictr_cliphandles::dragmode guictr_cliphandles::getDragZone(ivec2 local) {
    if (view.clip()) {
        struct dist_draghandle {
            float dist    = 0;
            dragmode mode = drag_handle_none;
        };

        float dragTop    = heightLoopInidicator / 2.0f;
        float dragBottom = dragTop + heightLoopInidicator;
        float distBar    = std::numeric_limits<float>::max();
        float barSX      = clipLoopStartScrX();
        float barEX      = clipLoopEndScrX();
        if (local.x >= barSX && local.x < barEX && local.y >= heightLoopInidicator && local.y < heightLoopInidicator * 2) {
            distBar = DRAG_RANGE * DRAG_RANGE * 0.8f;
        }
        std::vector<dist_draghandle> hndls{
            { dist(clipStartScrX(), dragTop, local), dragmode::drag_handle_left },
            { dist(clipEndScrX(), dragTop, local), dragmode::drag_handle_right },
            { dist(barSX, dragBottom, local), dragmode::drag_handle_loopleft },
            { dist(barEX, dragBottom, local), dragmode::drag_handle_loopright },
            { distBar, dragmode::drag_handle_loopbar }
        };
        std::sort(hndls.begin(), hndls.end(), [](dist_draghandle const& a, dist_draghandle const& b) {
            return a.dist < b.dist;
        });
        if (hndls[0].dist < DRAG_RANGE * DRAG_RANGE) {
            return hndls[0].mode;
        }
    }
    return drag_handle_none;
}

bool guictr_cliphandles::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 local = this->toContainerSpace(mpos);
        if (view.clip() && evt.type <= MouseHitType::MOUSE_RIGHT) {
            dragmode mode = getDragZone(local);
            if (mode == dragmode::drag_handle_loopleft || mode == dragmode::drag_handle_left) {
                evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
                evt.requestFocus(this);
                return true;
            }
            if (mode == dragmode::drag_handle_loopright || mode == dragmode::drag_handle_right) {
                evt.requestCursor(CURSOR_CLIP_SIZE_RIGHT);
                evt.requestFocus(this);
                return true;
            }
            if (mode == dragmode::drag_handle_loopbar) {
                evt.requestCursor(CURSOR_RESIZE_H);
                evt.requestFocus(this);
                return true;
            }
        }
    }
    return false;
}

void guictr_cliphandles::render(NVGcontext* vg) {
    ivec2 cs = clipViewSize;
    if (cs.y <= 0 || cs.x <= 0) {
        return;
    }
    MainCtrl* ctrl = MainCtrl::get();
    //DawInstance* daw = DawInstance::get();
    tick_t clipOffset = (view.clip()) ? view.clip()->getOffsetStart() : 0;
    nvgIntersectScissor(vg, pos.x, pos.y, cs.x, cs.y);
    nvgTranslate(vg, pos.x, pos.y);
    nvgBeginPath(vg);
    nvgRect(vg, -2, 0, cs.x + 2, size.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
    nvgFill(vg);
    //{
    //
    //    float w        = (float) size.x;
    //    float bgRepeat = grid.incr_bg * 2.0f;
    //    float bgOffset = (float) std::fmod(grid.offset, bgRepeat);
    //    int steps_bg   = math::ceildS32((w + bgRepeat) / grid.incr_bg);
    //    float x        = -bgOffset;
    //    for (int i = 0; i < steps_bg; i += 2) {
    //        nvgBeginPath(vg);
    //        nvgRect(vg, x, 0, grid.incr_bg, size.y);
    //        nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
    //        nvgFill(vg);
    //        x += grid.incr_bg * 2.0f;
    //        if (x > w)
    //            break;
    //    }
    //}
    for (grid_div g: grid.gridList) {
        nvgBeginPath(vg);
        nvgMoveTo(vg, g.screenpos, 0);
        nvgLineTo(vg, g.screenpos, heightLoopInidicator * 2);
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
    nvgBeginPath(vg);
    nvgRect(vg, -2, heightLoopInidicator * 2, cs.x + 2, heightSelIndicator);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
    nvgFill(vg);
    if (view.clip()) {
        const NVGcolor colLI        = GUI_COLOR(120);
        const NVGcolor colLIStroke  = theme->getFrameColorOutline();
        const float strokeWidthLI   = theme->getFloat(GuiConstant::CONST_CLIPEDITOR_HANDLES_STROKE_WIDTH);
        const float wLoopInidicator = heightLoopInidicator;

        float tickBeginX = clipStartScrX();
        float tickEndX   = clipEndScrX();

        int yOffset = 0;

        if (!(tickBeginX - wLoopInidicator > cs.x || tickEndX + wLoopInidicator < 0)) {
            float barBeginX = math::max(-wLoopInidicator, tickBeginX);
            float barEndX   = math::min(cs.x + wLoopInidicator, tickEndX);
            NVGcolor color  = rgbToNvg(view.clip()->rgb);
            nvgBeginPath(vg);
            nvgRect(vg, barBeginX, yOffset, barEndX - barBeginX, heightLoopInidicator * 2);
            nvgFillColor(vg, color);
            nvgFill(vg);
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

    /* render track-editor selection range in clipview */
    DAW::Cursor& c = MainCtrl::get()->getCursor();
    if (c.selRange) {
        int32_t tickBegin = c.getTickBegin() - clipOffset;
        int32_t tickEnd   = c.getTickEnd() - clipOffset;
        float tickBeginX  = (float) grid.tickToScreenD(tickBegin);
        float tickEndX    = (float) grid.tickToScreenD(tickEnd);
        if (tickEndX > -4.0f && tickBeginX < cs.x + 4.0f) {
            tickBeginX  = CLAMP_I(tickBeginX, -4.0f, cs.x + 3.0f);
            tickEndX    = CLAMP_I(tickEndX, -3.0f, cs.x + 4.0f);
            float width = (float) (tickEndX - tickBeginX);
            nvgBeginPath(vg);
            nvgRect(vg, (float) tickBeginX, heightLoopInidicator * 2.0f, width, heightSelIndicator);
            nvgFillColor(vg, G_SELECTION);
            nvgFill(vg);
        }
    }
    //-view.clip->start()+view.clip->offsetStart
    clip_t* clip = view.clip();
    if (clip) {
        tick_t pos = DawInstance::get()->getPlaybackPos() - clip->time + clip->offsetStart;
        if (clip->loopEnabled && clip->loopLen > 0) {
            if (pos > clip->loopStart) {
                pos = clip->loopStart + (pos - clip->loopStart) % clip->loopLen;
            }
        }
        float playBackX = (float) grid.tickToScreenD(pos);
        if (playBackX > -4.0f && playBackX < cs.x + 4.0f) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, playBackX, 0);
            nvgLineTo(vg, playBackX, cs.y);
            nvgStrokeColor(vg, GUI_COLOR(120));
            nvgStrokeWidth(vg, 3);
            nvgStroke(vg);
            nvgBeginPath(vg);
            nvgMoveTo(vg, playBackX, 0);
            nvgLineTo(vg, playBackX, cs.y);
            nvgStrokeColor(vg, GUI_COLOR(250));
            nvgStrokeWidth(vg, 1);
            nvgStroke(vg);
        }
    }
}
namespace GuiColor {
    constant_t COL_FOLD_BUTTON("COL_FOLD_BUTTON", 0xFFFF9933);
}

guictr_noteeditor::guictr_noteeditor(clip_view& _view)
    : guictr_base(), layout_pianoroll_t(),
      piano(_view, *this),
      content(grid, _view, *this),
      velocities(grid, _view, *this),
      timeline(grid),
      clipHandles(grid, _view), view(_view) {
    padding = 2;
    grid.showRange(0, TICKS_BAR * 4);
    grid.addCallback(this);
    add(&piano);
    add(&content);
    add(&velocities);
    add(&timeline);
    add(&clipHandles);
    add(&btnToggleFold);
    btnToggleFold.setButtonColor(GuiColor::COL_FOLD_BUTTON);
    btnToggleFold.setText("Fold");
    btnToggleFold.setStateRef(&fold);
    content.showRange(2 * 12, 4 * 12);
}

guictr_noteeditor::~guictr_noteeditor() {
    remove(&btnToggleFold);
    remove(&timeline);
    remove(&velocities);
    remove(&content);
    remove(&piano);
    remove(&clipHandles);
}

void guictr_noteeditor::buttonClicked(guibase* button) {
    if (button == &btnToggleFold) {
        fold = !fold;
        view.updateNotePitches(true);
        if (fold && yscalefold == 0 && yoffsetfold == 0) {
            zoomPianoRollToClipsNoteRange();
        }
    }
}

void guictr_noteeditor::renderBackground(NVGcontext* vg) {
    drawInsetBackground(vg, theme, getPosContent(), getSizeContent());
}

int32_t guictr_noteeditor::getTotalWidth() {
    return math::max(10000, getSizeContent().x);
}

void guictr_noteeditor::layout() {
    ivec2 cs = getSizeContent();

    if (size.y < velHeight * 2) {
        velocities.setVisible(false);
        piano.size      = ivec2(100, cs.y - heightTimeLine - heightClipIndicators);
        velocities.size = ivec2(timeline.size.x, 0);
    } else {
        velocities.setVisible(true);
        piano.size      = ivec2(100, cs.y - heightTimeLine - heightClipIndicators - velHeight);
        velocities.size = ivec2(timeline.size.x, velHeight);
    }

    piano.pos          = ivec2(0, heightTimeLine + heightClipIndicators);
    timeline.pos       = ivec2(piano.right(), 0);
    timeline.size      = ivec2(cs.x - piano.size.x, heightTimeLine);
    clipHandles.pos    = ivec2(timeline.left(), timeline.bottom());
    clipHandles.size   = ivec2(timeline.size.x, heightClipIndicators);
    btnToggleFold.pos  = ivec2(padding, padding);
    btnToggleFold.size = ivec2((piano.size.x) / 2, 18);
    content.pos        = ivec2(timeline.left(), clipHandles.bottom());
    content.size       = ivec2(timeline.size.x, piano.size.y);
    velocities.pos     = ivec2(timeline.left(), content.bottom());

    clipHandles.clipViewSize = ivec2(content.size.x, content.size.y + clipHandles.size.y);
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
    if (!clip) {
        content.showRange(2 * 12, 4 * 12);
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
    clip_t* clip = view.clip();
    if (clip != NULL) {
        if (clip->noLayout) {
            grid.showRange(clip->offsetStart, clip->offsetStart + clip->getLen());
            zoomPianoRollToClipsNoteRange();
        } else {
            clip_editor_layout_t& layout = clip->editorLayout;
            grid.setLayout(layout.layoutGrid);
            setLayout(layout.layoutPianoRoll);
        }
    }
}

void guictr_noteeditor::storeLayout() {
    clip_t* clip = view.clip();
    if (clip != NULL) {
        clip_editor_layout_t& layout = clip->editorLayout;
        layout.layoutPianoRoll       = *static_cast<layout_pianoroll_t*>(this);
        layout.layoutGrid            = grid;//TODO: add a cast to get rid of slicing warning
        clip->noLayout               = false;
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
    nvgSave(vg);
    velocities.render(vg);
    nvgRestore(vg);
    nvgSave(vg);
    clipHandles.render(vg);
    nvgRestore(vg);
    btnToggleFold.render(vg);
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
    //    int colorIdx = 0;
    //    static NVGcolor dbgcolorsa[5] = {
    //        nvgRGBA(255, 0, 0, 55),
    //        nvgRGBA(0, 255, 0, 55),
    //        nvgRGBA(0, 0, 255, 55),
    //        nvgRGBA(255, 0, 255, 55),
    //        nvgRGBA(255, 255, 0, 55)
    //    };

    if (waveformRef->rendered) {
        //        nvgBeginPath(vg);
        //        nvgRect(vg, 2, 2, size.x - 4, size.y - 4);
        //        NVGcolor bgWave = dbgcolorsa[colorIdx % 5];
        //        bgWave.a        = 0.3f;
        //        nvgFillColor(vg, bgWave);
        //        nvgFill(vg);
        waveformrender::getInstance()->draw(vg, waveformRef, size);
    }

    nvgRestore(vg);
}
void gui_audiocontent::render(NVGcontext* vg) {
    renderAudioClip(vg);
}
void gui_audiocontent::releaseRendered() {
    //my_printf("release %012x from releaseRendered()\n", waveformRef);
    dbgassert(waveformrender::getInstance()->isValid(waveformRef));
    waveformrender::getInstance()->release(waveformRef);
    //m_clip->audio.waveformRef.fbId = -1;
    waveformRef->rendered = false;
}

audioclip_texture_t makeWaveformFromSample(const project_globals_t& project, scaled_grid& grid, const clip_audio_t& clipAudio,
                                           const ivec2& pos, const ivec2& size) {


    samplerate_t sr = vsthost::getInstance()->m_sampleFormatInternal.sampleRate;

    int32_t pxBegin        = 0;
    int32_t pxEnd          = size.x;

    double tickBeginOffset = grid.screenToTickD(0);
    double tickEnd         = grid.screenToTickD(pxEnd);

    double sampleStartOffset = tickToSampleConvert<double, roundmode::floor>(tickBeginOffset, project.tempo100, sr);
    double sampleEnd         = tickToSampleConvert<double, roundmode::floor>(tickEnd, project.tempo100, sr);

    double lenSamples   = sampleEnd - sampleStartOffset;
    double samplesPerPx = lenSamples / size.x;


    audioclip_texture_t w;
    w.quality = 2;

    double pxPerSample      = 1.0 / samplesPerPx;
    constexpr double MAX_RES = 2048.0;
    constexpr double FBO_WIDTH_D = FBO_WIDTH;

    w.scaleX = 1.0f;
    w.pos    = pos;
    w.size   = ivec2(math::min(size.x, FBO_WIDTH), math::min(size.y, FBO_HEIGHT));

    double nSamplesD = sampleEnd - sampleStartOffset;
    if (nSamplesD * pxPerSample > FBO_WIDTH_D) {
        samplesPerPx = (nSamplesD / FBO_WIDTH_D);
    }
    if (samplesPerPx > MAX_RES && (nSamplesD / MAX_RES) <= FBO_WIDTH_D) {
        w.scaleX     = static_cast<float>(MAX_RES / samplesPerPx);
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
    //log_printf("waveform %d - %d - %d - %d %f %f %f\n", w.audioId, w.sampleBegin, w.sampleBeginOffset, w.sampleEnd, w.samplesPerPx, grid.zoom, lenSamples);
    //log_printf("waveform[height:%d,zoom:%f,q:%d,w:%f,smp/px:%f,scale:%f]\n", w.size.y, grid.zoom, w.quality, w.linewidth, w.samplesPerPx, w.scaleX);


    return w;
}
inline bool isAlmostEqualWaveformSample(const audioclip_texture_t& lhs, const audioclip_texture_t& rhs) {
    if ((lhs.sampleBeginOffset - lhs.sampleBegin) == (rhs.sampleBeginOffset - rhs.sampleBegin) &&
        (lhs.sampleEnd - lhs.sampleBegin) == (rhs.sampleEnd - rhs.sampleBegin) &&
        //lhs.startOffset == rhs.startOffset &&
        //lhs.size == rhs.size &&
        //lhs.samplesPerPx == rhs.samplesPerPx &&
        //lhs.scale == rhs.scale &&
        //lhs.scaleX == rhs.scaleX &&
        lhs.audioId == rhs.audioId && lhs.quality == rhs.quality && lhs.method == rhs.method) {

        if (lhs.clipped || rhs.clipped)
            return lhs.scaleX == rhs.scaleX && lhs.scaleY == rhs.scaleY && lhs.size == rhs.size && lhs.samplesPerPx == rhs.samplesPerPx;
        vec2 sd    = vec2(math::absvec2(lhs.size - rhs.size));
        vec2 limit = vec2(lhs.size) / 4.0f;
        return sd.x < limit.x && sd.y < limit.y;
    }
    return false;
}
void gui_audiocontent::updatePosition() {
    project_globals_t& project = DawInstance::get()->getGlobals();
    const clip_t* clip         = view.clip();
    if (!clip || clip->clipType != CLIP_AUDIO) {
        releaseRendered();
        return;
    }
    auto& clipAudio    = clip->audio;
    audiofile_t* audio = audiocache::getInstance()->get(clipAudio.id);
    if (!audio) {
        releaseRendered();
    }

    dbgassert(size.x > 0);
    if (audio) {
        audioclip_texture_t waveform = makeWaveformFromSample(project, grid, clipAudio, ivec2(0, 0), size);
        if (waveform.size.x < 1 || waveform.size.y < 1) {
            releaseRendered();
            waveformRef->waveform = waveform;
            this->updatedWaveform = waveform;
        } else if (waveformrender::getInstance()->canQueueUpdate()) {
            bool equal = waveform.size == waveformRef->waveform.size && clipAudio.id == waveformRef->waveform.audioId && isAlmostEqualWaveformSample(waveform, waveformRef->waveform);
            if (!equal) {
                this->updatedWaveform = waveform;
            }
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
    const clip_t* clip = view.clip();
    if (!clip || clip->clipType != CLIP_AUDIO) {
        return;
    }
    auto& clipAudio    = clip->audio;
    audiofile_t* audio = audiocache::getInstance()->get(clipAudio.id);
    if (!waveformRef->queued) {
        if (!audio || this->updatedWaveform.size.x < 1 || this->updatedWaveform.size.y < 1) {
            return;
        }
        if ((!waveformRef->rendered || (this->updatedWaveform != waveformRef->waveform))) {
            releaseRendered();
            dbgassert(!waveformRef->rendered && !waveformRef->queued);
            waveformRef->waveform = this->updatedWaveform;
            dbgassert(!waveformRef->queued);
            dbgassert(waveformRef->waveform.size.x > 0 && waveformRef->waveform.size.y > 0);
            if (waveformrender::getInstance()->queueUpdate(audio, waveformRef)) {
                dbgassert(!waveformRef->rendered && waveformRef->queued);
                dbgassert(waveformrender::getInstance()->isValid(waveformRef));
            }
        }
    }
}

void gui_audiocontent::layout() {
    for (guibase* gui: guis) {
        gui->layout();
    }
}


guictr_audioeditor::guictr_audioeditor(clip_view& _view)
    : guictr_base(),
      content(grid, _view),
      timeline(grid),
      clipHandles(grid, _view),
      view(_view) {
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
    clipHandles.pos  = ivec2(timeline.left(), timeline.bottom());
    clipHandles.size = ivec2(timeline.size.x, heightClipIndicators);
    content.pos      = ivec2(timeline.left(), clipHandles.bottom());
    content.size     = ivec2(timeline.size.x, cs.y - heightTimeLine - heightClipIndicators);

    clipHandles.clipViewSize = ivec2(content.size.x, content.size.y + clipHandles.size.y);
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
    clip_t* clip = view.clip();
    if (clip != NULL) {
        if (clip->noLayout) {
            grid.showRange(clip->offsetStart, clip->offsetStart + clip->getLen());
        } else {
            clip_editor_layout_t& layout = clip->editorLayout;
            grid.setLayout(layout.layoutGrid);
        }
    }
    content.updatePosition();
}

void guictr_audioeditor::storeLayout() {
    clip_t* clip = view.clip();
    if (clip != NULL) {
        clip_editor_layout_t& layout = clip->editorLayout;
        layout.layoutGrid            = grid;//TODO: add a cast to get rid of slicing warning
        clip->noLayout               = false;
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
    nvgSave(vg);
    clipHandles.render(vg);
    nvgRestore(vg);
}
