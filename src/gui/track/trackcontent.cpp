#include <deque>
#include <glm/geometric.hpp>
#include <memory>
#include <nanovg.h>
#include <numeric>
#include <vector>

#include "assert_dbg.h"
#include "event.h"
#include "gui/clipeditor/clipeditor.h"
#include "gui/controls/button.h"
#include "gui/dropdown/dropdown.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "gui/table/table.h"
#include "gui/tooltip/tooltip.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/contextmenu/contextmenu_grid.h"

#include "basectrl.h"
#include "host/daw/mainctrl.h"

#include "keyboard.h"
#include "logging.h"
#include "mouse.h"
#include "seq_time.h"
#include "host/shape/shape.h"
#include "host/track/track.h"
#include "trackautomation.h"
#include "host/track/track_impl.h"

#include "samplerate.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "host/audiocache/audiocache.h"
#include "wave/waveform_render.h"
#include "wave/waveform_render_impl.h"
#include "host/host_pluginmanager.h"
#include "gui/shape/shapeeditor.h"

struct track_gui_entry_t;

constexpr int32_t CLIPPING_STEP_PX = 512;
constexpr int32_t MARGIN_CLIPPING_PX = 32;
constexpr int32_t MIN_WIDTH_CLIP_FADES = 6;


bool getClipPositionFloat(scaled_grid& grid, const ivec2& scissorSize, const clip_t* cl, vec2& pos, vec2& size, double tickOffset, const float minWidth) {
    double tickBegin  = cl->time + tickOffset;
    double tickEnd    = cl->time + tickOffset + cl->getLen();
    double tickBeginX = grid.tickToScreenD(tickBegin);
    double tickEndX   = grid.tickToScreenD(tickEnd);
    if (tickEndX < -MARGIN_CLIPPING_PX || tickBeginX > scissorSize.x + MARGIN_CLIPPING_PX) {
        return false;
    }
    double width = tickEndX - tickBeginX;
    dbgassert(FitsTypeRange<int32_t>(tickBeginX));
    dbgassert(FitsTypeRange<int32_t>(tickEndX));
    if (width >= minWidth) {
        pos  = vec2(tickBeginX, INSET_TRACK_CONTENT);
    } else {
        width = minWidth;
        double tickCenterX = grid.tickToScreenD((tickBegin + tickEnd) * 0.5);
        pos  = vec2(tickCenterX - width * 0.5f, INSET_TRACK_CONTENT);
    }
    size = math::maxvec2f(vec2(width, size.y - INSET_TRACK_CONTENT * 2), vec2(0));
    return size.x > 0.5f && size.y > 0.5f;
}
bool getClipPositionInt(scaled_grid& grid, const ivec2& scissorSize, const clip_t* cl, ivec2& pos, ivec2& size, double tickOffset, const float minWidth) {
    double tickBegin  = cl->time + tickOffset;
    double tickEnd    = cl->time + tickOffset + cl->getLen();
    double tickBeginX = grid.tickToScreenD(tickBegin);
    double tickEndX   = grid.tickToScreenD(tickEnd);
    if (tickEndX < -MARGIN_CLIPPING_PX || tickBeginX > scissorSize.x + MARGIN_CLIPPING_PX) {
        return false;
    }
    double width = tickEndX - tickBeginX;
    dbgassert(FitsTypeRange<int32_t>(tickBeginX));
    dbgassert(FitsTypeRange<int32_t>(tickEndX));
    if (width >= minWidth) {
        pos  = ivec2(math::rounddS32(tickBeginX), INSET_TRACK_CONTENT);
    } else {
        width = minWidth;
        double tickCenterX = grid.tickToScreenD((tickBegin + tickEnd) * 0.5);
        pos  = ivec2(math::rounddS32(tickCenterX - width * 0.5f), INSET_TRACK_CONTENT);
    }
    size = math::maxvec2(ivec2(math::rounddS32(width), size.y - INSET_TRACK_CONTENT * 2), ivec2(0));
    return size.x > 0 && size.y > 0;
}

bool getClippedPosSize(const ivec2& parentSize, ivec2& posClipped, ivec2& sizeClipped) {
    bool wasClipped = false;

    // apply clipping in steps
    if (posClipped.x < -MARGIN_CLIPPING_PX) {
        auto clippingLen = (static_cast<int32_t>(-(posClipped.x + MARGIN_CLIPPING_PX)) / CLIPPING_STEP_PX) * CLIPPING_STEP_PX;
        posClipped.x += clippingLen;
        sizeClipped.x -= clippingLen;
        wasClipped = true;
    }

    if (posClipped.x + sizeClipped.x > parentSize.x + MARGIN_CLIPPING_PX) {
        auto over = static_cast<int32_t>((posClipped.x + sizeClipped.x) - (parentSize.x + MARGIN_CLIPPING_PX));
        auto clippingLen = (over / CLIPPING_STEP_PX) * CLIPPING_STEP_PX;
        sizeClipped.x -= clippingLen;
        wasClipped = true;
    }

    return wasClipped;
}

namespace DAW {
    gui_track* createTrackGui(track_gui_entry_t* _entry, scaled_grid& grid) {
        auto* const guitrack = new gui_track(_entry, grid);
        guitrack->setZOrder(TRACKTYPE_TO_CTR(_entry->track->type) == TRACK_CTR_MIDIAUDIO ? 0 : 1);
        return guitrack;
    }

    gui_track_controls* createTrackGuiMixer(track_gui_entry_t* _entry, scaled_grid& grid) {
        auto const guicontrols = new gui_track_controls(_entry, grid);
        guicontrols->setZOrder(_entry->track->type >= TRACK_TYPE_MIDI ? 0 : 1);
        return guicontrols;
    }

    gui_clip* createClipGui(guictr_base* parent, track_gui_entry_t* trackentry, clip_t* clip) {
        auto waveformRenderer = parent->dawCtrl->getWaveformRenderer();
        if (0 == trackentry->clipsGuis.count(clip)) {
            if (clip->clipType == CLIP_MIDI) {
                trackentry->clipsGuis[clip] = new gui_midi_clip(trackentry, clip);
            } else {
                trackentry->clipsGuis[clip] = new gui_audio_clip(trackentry, clip, waveformRenderer);
            }
            clip->trackEntries.push_back(trackentry);
        }
        return trackentry->clipsGuis[clip];
    }
}

gui_audio_clip::gui_audio_clip(track_gui_entry_t* _track, clip_t* _clip, waveformrender* _waveformRenderer)
    : gui_clip(_track, _clip),
    rendered_audio_clip_t(_waveformRenderer)
{

}

gui_audio_clip::~gui_audio_clip() {
}

void gui_audio_clip::onRemove() {
    releaseWaveformTexture();
    dbgassert(STL_CONTAINS(m_clip->trackEntries, this->m_trackentry));
    removeEntry(this->m_clip->trackEntries, this->m_trackentry);
    auto it2 = m_trackentry->clipsGuis.find(m_clip);
    dbgassert(it2 != m_trackentry->clipsGuis.end());
    m_trackentry->clipsGuis.erase(it2);
}

void gui_midi_clip::onRemove() {
    dbgassert(STL_CONTAINS(m_clip->trackEntries, this->m_trackentry));
    removeEntry(this->m_clip->trackEntries, this->m_trackentry);
    auto it1 = m_trackentry->clipsGuis.find(m_clip);
    dbgassert(it1 != m_trackentry->clipsGuis.end());
    m_trackentry->clipsGuis.erase(it1);
}

void gui_audio_clip::renderDebugPass(NVGcontext* vg) {
    if (!culled) {
        const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        ivec2 shrink = ivec2(0, (HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2));
        ivec2 sizeClipped = size - shrink;
        ivec2 posClipped = pos + shrink;

        getClippedPosSize(parent->size, posClipped, sizeClipped);
        if (sizeClipped.x > 0 && sizeClipped.y > 0) {
            gui_waveform_texture_ref* ref = getWaveformTextureRef();
            auto file = dawCtrl->getDaw()->getAudioCache()->get(m_clip->audio.id);
            renderAudioClip(vg, dawCtrl->getWaveformRenderer(), theme, m_track, m_clip, file, ref, pos, size, posClipped, sizeClipped);
            nvgBeginPath(vg);
            nvgRect(vg, posClipped.x, posClipped.y, sizeClipped.x, sizeClipped.y);
            nvgFillColor(vg, rgbaToNvg(0x7Fff00ff));
            nvgFill(vg);
        }

    }
}
DAW::Shape::ShapeEdit& gui_audio_clip::createShapeEdit() {
    if (!editState) {
        editState = std::make_unique<edit_state_t>();
    }
    return editState->shapeEdit;
}
DAW::Shape::ShapeEdit* gui_audio_clip::getShapeEdit() {
    return editState ? &editState->shapeEdit : nullptr;
}

void gui_audio_clip::render(NVGcontext* vg) {
    if (!culled) {
        const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        ivec2 shrink = ivec2(0, (HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2));
        ivec2 sizeClipped = size - shrink;
        ivec2 posClipped = pos + shrink;

        getClippedPosSize(parent->size, posClipped, sizeClipped);

        gui_waveform_texture_ref* ref = getWaveformTextureRef();
        auto file = dawCtrl->getDaw()->getAudioCache()->get(m_clip->audio.id);
        renderAudioClip(vg, dawCtrl->getWaveformRenderer(), theme, m_track, m_clip, file, ref, pos, size, posClipped, sizeClipped);
        if (fadeInLayout.size.y < 5 || sizeClipped.x < 1 || sizeClipped.y < 1) {
            return;
        }
        const auto relMousepos = toControlsObjectSpace(parentCtrl->m_mousePos, parent);
        for (auto* fadeLayout : { &fadeInLayout, &fadeOutLayout }) {
            const uint8_t fadeIdx = fadeLayout == &fadeInLayout ? 0 : 1;
            using DAW::Shape::shape_t;
            using hittype = DAW::Shape::shape_t::hittype;
            const vec2 editRelMouse = vec2(relMousepos) - fadeLayout->pos;
            bool bRenderedEdit = false;
            if (editingFade == fadeIdx) {
                auto shapeEdit = getShapeEdit();
                if (shapeEdit && shapeEdit->dragged.type != hittype::HIT_NONE) {
                    shapeEdit->renderEditor(vg, fadeLayout->pos, theme, editRelMouse, true);
                    bRenderedEdit = true;
                }
            }
            const bool bContained = this->contains(relMousepos) && editRelMouse.y >= 0 && editRelMouse.y < fadeLayout->size.y;
            const auto& shape = *fadeLayout->fade.shape;
            const float xDragEdge = (1.0f-fadeIdx) * fadeLayout->size.x;
            vec2 sPos = fadeLayout->pos;
            vec2 sSize = fadeLayout->size;
            bool bRenderShape = fadeLayout->fade.hasFade();
            auto col = GuiColor::COL_CLIP_FADES;
            auto col2 = GuiColor::COL_SHAPE_CURVE_HIGHLIGHT;
            if (bContained && math::abs(editRelMouse.x - xDragEdge) < DAW::Shape::GetMinDistEdgeMouseHit()) {
                float strokeWidth = 3.0f;
                auto pt0 = fadeLayout->pos + vec2((1-fadeIdx), 0) * fadeLayout->size;
                auto pt1 = fadeLayout->pos + vec2((1-fadeIdx), 1) * fadeLayout->size;
                nvgBeginPath(vg);
                nvgMoveTo(vg, pt0.x, pt0.y);
                nvgLineTo(vg, pt1.x, pt1.y);
                nvgStrokeColor(vg, theme->getColor(col2));
                nvgStrokeWidth(vg, strokeWidth);
                nvgStroke(vg);
                bRenderShape = true;
                if (sSize.x < 12) {
                    if (fadeIdx&&size.x > 12) {
                        sPos.x-=12;
                        sSize.x+=12;
                    } else {
                        sSize.x+=12;
                    }
                }
                col = GuiColor::COL_SHAPE_CURVE_HIGHLIGHT;
            } else {
                if (fadeLayout->size.x < MIN_WIDTH_CLIP_FADES || fadeLayout->size.y < MIN_WIDTH_CLIP_FADES) {
                    continue;
                }
            }
            
            if (!bRenderedEdit && bRenderShape) {
                const auto mousePosScaledToFade = viewToCtrlPt(editRelMouse, fadeLayout->size);
                auto result = !bContained ? DAW::Shape::shape_t::hit_result() : shape.getMouseHit(mousePosScaledToFade, fadeLayout->size);
                if (!bContained || result.type != hittype::HIT_EDGE) {
                    result.type = hittype::HIT_NONE;
                }
                
                DAW::Shape::DrawShapeOneShot(*fadeLayout->fade.shape, vg, theme, col, col2, sPos, sSize, -0.1f, 1.1f, result);
            }
        }
    }
}
bool gui_audio_clip::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (culled) {
        return false;
    }
    if (gui_clip::mouseHitTest(mpos, evt)) {
        return true;
    }
    if (this->contains(mpos) && fadeInLayout.size.y > 5) {
        for (auto* fadeLayout : { &fadeInLayout, &fadeOutLayout }) {
            const uint8_t fadeIdx = fadeLayout == &fadeInLayout ? 0 : 1;
            const auto editRelMouse = vec2(mpos) - fadeLayout->pos;
            if (editRelMouse.y < 0 || editRelMouse.y >= fadeLayout->size.y) {
                continue;
            }
            const auto mousePosScaledToFade = viewToCtrlPt(editRelMouse, fadeLayout->size);
            const float xDragEdge = (1.0f-fadeIdx) * fadeLayout->size.x;
            const float distAbs = math::abs(editRelMouse.x - xDragEdge);
            if (distAbs < DAW::Shape::GetMinDistEdgeMouseHit()) {
                evt.requestFocus(this);
                evt.requestCursor(CURSOR_RESIZE_H);
                return true;
            }
            if (fadeLayout->size.x < MIN_WIDTH_CLIP_FADES || fadeLayout->size.y < MIN_WIDTH_CLIP_FADES) {
                continue;
            }
            if (fadeLayout->fade.hasFade()) {
                auto& shape = *fadeLayout->fade.shape;
                auto higlightHit = shape.getMouseHit(mousePosScaledToFade, fadeLayout->size);
                if (higlightHit.type == DAW::Shape::shape_t::hittype::HIT_EDGE) {
                    evt.requestFocus(this);
                    evt.requestCursor(CURSOR_RESIZE_V);
                    return true;
                }
            }
        }
    }
    return false;
}

void gui_audio_clip::handleDraggedBegin(MouseEvent& evt) {
    auto shapeEdit = getShapeEdit();
    if (shapeEdit && shapeEdit->dragged.type != DAW::Shape::shape_t::hittype::HIT_NONE) {
        shapeEdit->dragged = {};
    }
    auto daw = dawCtrl->getDaw();
    for (auto* fadeLayout : { &fadeInLayout, &fadeOutLayout }) {
        const uint8_t fadeIdx = fadeLayout == &fadeInLayout ? 0 : 1;
        const auto editRelMouse = vec2(evt.relMousepos + pos) - fadeLayout->pos;
        if (editRelMouse.y < 0 || editRelMouse.y >= fadeLayout->size.y) {
            continue;
        }
        const auto mousePosScaledToFade = viewToCtrlPt(editRelMouse, fadeLayout->size);
        auto& shape = *fadeLayout->fade.shape;
        auto dragged = shape.getMouseHit(mousePosScaledToFade, fadeLayout->size);
        const float xDragEdge = (1.0f-fadeIdx) * fadeLayout->size.x;
        const float distAbs = math::abs(editRelMouse.x - xDragEdge);
        bool bBeginEdit = false;
        MouseEvent evtOffset = evt;
        evtOffset.relMousepos = editRelMouse;
        if (distAbs < DAW::Shape::GetMinDistEdgeMouseHit()) {
            bBeginEdit = true;
            evtOffset.relMousepos.y = 0;
            evtOffset.kbmods = KeyboardMods::KB_MODS_NONE;
        } else if (fadeLayout->fade.hasFade()) {
            if (fadeLayout->size.x < MIN_WIDTH_CLIP_FADES || fadeLayout->size.y < MIN_WIDTH_CLIP_FADES) {
                continue;
            }
            if (dragged.type == DAW::Shape::shape_t::hittype::HIT_EDGE) {
                bBeginEdit = true;
                evtOffset.kbmods = KeyboardMods::KB_MOD_ALT;
            }
        }
        if (bBeginEdit) {
            this->editingFade = fadeIdx;
            auto& shapeEdit = createShapeEdit();
            shapeEdit.setEditorCurve(&m_clip->getFade(editingFade).shape);
            shapeEdit.layoutEditor(fadeLayout->size);
            shapeEdit.onBeginDragCurveEditor(evtOffset);
            editState->dataBefore = m_clip->audio;
            editor_view_selection_t view;
            DAW::GetClipboardView(m_trackentry->parent->guiMgr, dawCtrl->getCursor(), view, this);
            daw->setEditorSelection(m_clip, view);
            return;
        }
    }
    evt.relMousepos += pos;
    parent->handleDraggedBegin(evt);
}

void gui_audio_clip::handleDraggedMove(MouseEvent& evt) {
    auto shapeEdit = getShapeEdit();
    if (shapeEdit && shapeEdit->dragged.type == DAW::Shape::shape_t::hittype::HIT_NODE) {
        auto mousePosTrackCtr = toControlsObjectSpace(evt.mousepos, parent);
        double tick = m_trackentry->parent->m_grid.screenToTickD(mousePosTrackCtr.x);
        
        auto& fadeToEdit = m_clip->getFade(editingFade);
        double relative = math::clamp<double>(tick - m_clip->start(), 0, m_clip->getLen());
        if (editingFade == 1) {
            relative = math::clamp<double>(m_clip->end() - tick, 0, m_clip->getLen());
        }
        auto sr = m_trackentry->track->audio->sampleFormat;
        auto tempo100 = m_trackentry->parent->projectGlobals.tempo100;
        double lenSamples = tickToSampleConvert<double, roundmode::none>(relative, tempo100, sr.sampleRate);
        double asMs = (lenSamples / sr.sampleRate) * 1000.0;
        fadeToEdit.durationMs = asMs;
        m_trackentry->parent->layoutVisibleTracks();
        return;
    }
    if (shapeEdit && shapeEdit->dragged.type == DAW::Shape::shape_t::hittype::HIT_EDGE) {
        MouseEvent evtOffset = evt;
        evtOffset.relMousepos = vec2(evt.relMousepos + pos) - (getFadeLayout(editingFade).pos);
        shapeEdit->onMoveDragCurveEditor(evtOffset);
        m_trackentry->parent->layoutVisibleTracks();
        return;
    }
    evt.relMousepos += pos;
    parent->handleDraggedMove(evt);
}

void gui_audio_clip::handleDraggedRelease(MouseEvent& evt) {
    bool bHasEdit = false;
    auto shapeEdit = getShapeEdit();
    if (shapeEdit && shapeEdit->dragged.type == DAW::Shape::shape_t::hittype::HIT_NODE) {
        bHasEdit = true;
    }
    if (shapeEdit && shapeEdit->dragged.type == DAW::Shape::shape_t::hittype::HIT_EDGE) {
        MouseEvent evtOffset = evt;
        evtOffset.relMousepos = vec2(evt.relMousepos + pos) - (getFadeLayout(editingFade).pos);
        shapeEdit->onReleaseDragCurveEditor(evtOffset);
        bHasEdit = true;
    }
    if (bHasEdit) {
        shapeEdit->dragged = {};
        m_trackentry->parent->layoutVisibleTracks();
        bRequestRefresh = true;
        auto daw = dawCtrl->getDaw();
        auto clipBefore = *m_clip;
        clipBefore.audio = editState->dataBefore;
        String desc = "Edit Clip Fade" + String(editingFade == 0 ? " In" : " Out");
        daw->pushHist(new action_modify_clip(desc, m_trackentry->track, *m_clip, &clipBefore));
        m_clip->setDirty();
        return;
    }
    evt.relMousepos += pos;
    parent->handleDraggedRelease(evt);
}


void gui_midi_clip::handleRightClick(MouseEvent& evt) {
    auto trackEditor = guiParentType<guitrack_editor, gui_type::CTR_TYPE_TRACKS_EDITOR>(this->parent);
    if (!assert_expr(trackEditor)) {
        return;
    }
    parentCtrl->openContextMenu(new guictxtmenu_clip(trackEditor, this), evt.mousepos);
}

void gui_audio_clip::handleRightClick(MouseEvent& evt) {
    for (auto* fadeLayout : { &fadeInLayout, &fadeOutLayout }) {
        const uint8_t fadeIdx = fadeLayout == &fadeInLayout ? 0 : 1;
        if (fadeLayout->fade.hasFade()) {
            const auto editRelMouse = vec2(evt.relMousepos + pos) - fadeLayout->pos;
            const auto mousePosScaledToFade = viewToCtrlPt(editRelMouse, fadeLayout->size);
            auto& shape = *fadeLayout->fade.shape;
            auto dragged = shape.getMouseHit(mousePosScaledToFade, fadeLayout->size);
            if (dragged.type != DAW::Shape::shape_t::hittype::HIT_NONE) {
                auto& shapeEdit = createShapeEdit();
                shapeEdit.setEditorCurve(&m_clip->getFade(fadeIdx).shape);
                shapeEdit.layoutEditor(fadeLayout->size);
                MouseEvent evtOffset = evt;
                evtOffset.relMousepos = editRelMouse;
                shapeEdit.onRightClickCurveEditor(evtOffset);
                return;
            }
        }
    }
    auto trackEditor = guiParentType<guitrack_editor, gui_type::CTR_TYPE_TRACKS_EDITOR>(this->parent);
    if (!assert_expr(trackEditor)) {
        return;
    }
    parentCtrl->openContextMenu(new guictxtmenu_clip(trackEditor, this), evt.mousepos);
}

void gui_audio_clip::updateClipRenderCache(NVGcontext* vg) {
}

void gui_audio_clip::updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) {
    size   = this->parent->size;
    culled = !getClipPositionInt(grid, trackSize, m_clip, pos, size, 0);
    auto daw = dawCtrl->getDaw();
    auto prjGlobals = daw->getProjectGlobals();
    auto cache = daw->getAudioCache();
    audiofile_t* audio = cache->get(m_clip->audio.id);

    if (culled || !audio) {
        releaseWaveformTexture();
        return;
    }

    dbgassert(size.x > 0);

    const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    ivec2 shrink = ivec2(0, (HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2));
    ivec2 sizeClipped = size - shrink;
    ivec2 posClipped = pos + shrink;
    auto fadeIn = m_clip->getSampleFadeIn(prjGlobals.tempo100, audio->sample->sampleRate);
    auto fadeOut = m_clip->getSampleFadeOut(prjGlobals.tempo100, audio->sample->sampleRate);
    this->fadeInLayout = { };
    this->fadeOutLayout = { };
    for (auto* fadeRef : { &fadeIn, &fadeOut }) {
        const uint8_t fadeIdx = fadeRef == &fadeIn ? 0 : 1;
        if (fadeRef->hasFade()) {
        }
        auto dTick = sampleToTickConvert<double, roundmode::none>(fadeRef->samplesFadePos, prjGlobals.tempo100, audio->sample->sampleRate);
        auto dTickEnd = sampleToTickConvert<double, roundmode::none>(fadeRef->samplesFadePos + fadeRef->samplesFadeDuration, prjGlobals.tempo100, audio->sample->sampleRate);
        auto beginX = grid.tickToScreenD(m_clip->start() + dTick);
        auto endX = grid.tickToScreenD(m_clip->start() + dTickEnd);
        auto& layout = getFadeLayout(fadeIdx);
        layout.pos = ivec2(beginX, posClipped.y);
        layout.size = ivec2(endX - beginX, sizeClipped.y);
        layout.fade = *fadeRef;
    }
    auto shapeEdit = getShapeEdit();
    if (shapeEdit && shapeEdit->dragged.type != DAW::Shape::shape_t::hittype::HIT_NONE) {
        shapeEdit->layoutEditor(getFadeLayout(editingFade).size);
    }

    getClippedPosSize(parent->size, posClipped, sizeClipped);

    if (posClipped.x + sizeClipped.x <= 0 || sizeClipped.x <= 0) {
        releaseWaveformTexture();
        culled = true;
        return;
    }

    const auto tempo100 = project.tempo100;
    const auto samplerate = m_track->audio->sampleFormat.sampleRate;
    auto waveform = makeWaveformFromClip(tempo100, samplerate, grid, trackSize, m_clip, pos, size - shrink, posClipped, sizeClipped);
    if (waveform.size.x < 1 || waveform.size.y < 1) {
        releaseWaveformTexture();
        updateWaveformTexture(waveform);
        return;
    }
    auto& currentWaveformShape = getCurrentWaveformShape();
    bool equal = ((waveform.size.y > 0) == (currentWaveformShape.size.y > 0)) && isEqualWaveform3(waveform, currentWaveformShape);

    bool canQueue  = getWaveformRenderer()->canQueueUpdate();
    ivec2 sizeDiff = math::absvec2(waveform.size - currentWaveformShape.size);
    ivec2 limit    = math::maxvec2(ivec2(1), ivec2(waveform.size.x / 4, 16));
    if (!canQueue) {
        limit.x = waveform.size.x / 4;
    }
    if (waveform.clipped || (dawCtrl && !dawCtrl->isZooming())) {
        limit = { 0, 0 };
    }
    if (!equal || (sizeDiff.x > limit.x || sizeDiff.y > limit.y)) {
        updateWaveformTexture(waveform);
        if (sizeDiff.x > limit.x || sizeDiff.y > limit.y) {
            //releaseWaveformTexture();
        }
    }
}

void gui_track::prerender(NVGcontext* vg) {
	nvgReset(vg);
    nvgScale(vg, parentCtrl->m_scale, parentCtrl->m_scale);
    nvgLineJoin(vg, NVGlineCap::NVG_BEVEL);
    nvgCachePath(vg, 1);
    for (auto& entry : m_trackentry->clipsGuis) {
        if (entry.second) {
            entry.second->updateClipRenderCache(vg);
        }
    }
    nvgCachePath(vg, 0);
    for (guibase* gui : guis) {
        gui->prerender(vg);
    }
}


rendered_audio_clip_t::rendered_audio_clip_t(waveformrender* waveformRenderer)
    : waveformRenderer(waveformRenderer), waveformRef(new gui_waveform_texture_ref{}),
    tempWaveformRef(new gui_waveform_texture_ref{})
{

}

rendered_audio_clip_t::~rendered_audio_clip_t() {
    releaseWaveformTexture();
    delete waveformRef;
    delete tempWaveformRef;
}

void rendered_audio_clip_t::updateClipPrerender(NVGcontext* vg, clip_t* clip, audiofile_t* audio, bool culled) {
    if (!waveformRef->queued) {
        if (!audio || this->updatedWaveform.size.x < 1 || this->updatedWaveform.size.y < 1) {
            return;
        }
        if (!culled && !waveformRef->queued && (!waveformRef->rendered || (this->updatedWaveform != waveformRef->waveform))) {
            //releaseWaveformTexture();
            //dbgassert(!waveformRef->rendered && !waveformRef->queued);
            this->prevWaveform = waveformRef->waveform;
            this->prevIsValid = waveformRef->rendered;
            waveformRef->waveform = this->updatedWaveform;
            //dbgassert(!waveformRef->queued);
            dbgassert(waveformRef->waveform.size.x > 0 && waveformRef->waveform.size.y > 0);
            if (waveformRenderer->queueUpdate(audio, waveformRef)) {
                dbgassert(/*!waveformRef->rendered && */waveformRef->queued);
            }
        }
    }
    else if (waveformRef->rendered)
        prevIsValid=false;
}
gui_waveform_texture_ref* rendered_audio_clip_t::getWaveformTextureRef() {
    if (prevIsValid && waveformRef->queued) {
        *tempWaveformRef = *waveformRef;
        tempWaveformRef->waveform = prevWaveform;
        return tempWaveformRef;
    }
    return waveformRef;
}
const audioclip_texture_t& rendered_audio_clip_t::getCurrentWaveformShape() {
    return updatedWaveform;
}
void rendered_audio_clip_t::updateWaveformTexture(const audioclip_texture_t& newShape) {
    updatedWaveform = newShape;
}
void rendered_audio_clip_t::releaseWaveformTexture() {
    if (waveformRef->rendered || waveformRef->queued) {
        waveformRenderer->release(waveformRef);
    }
    waveformRef->queued = false;
    waveformRef->rendered = false;
}

void gui_audio_clip::prerender(NVGcontext* vg) {
    auto& clipAudio    = m_clip->audio;
    audiofile_t* audio = dawCtrl->getDaw()->getAudioCache()->get(clipAudio.id);
    updateClipPrerender(vg, m_clip, audio, culled);
}

using Table::table_entry_t;
using Table::tbl;
using Table::tbl_row_t;
using Table::tblfloat;
using Table::tblint;
using Table::tblstr;
using Table::tblString;

template<>
void guitooltip<clip_t>::setContent() {
    table.tableWidth = 400;
    using tbl_rows = std::vector<table_entry_t>;
    {
        //TODO: fix dawCtrl in tooltips/popups
        audiofile_t* c = audiocache::getInstance()->get(ptr->audio.id);

        String path;
        if (c) {
            path = StringFormat("%s.%s", StringAsCStr(c->name), StringAsCStr(c->ext));
        } else {
            path = StringFormat("<MISSING SAMPLE %d>", ptr->audio.id);
        }
        tbl_rows vec{ tblString{ StringFormat("Audio Clip (sample-id %d)", ptr->audio.id) }, tblString{ path } };
        table.rows.push_back(tbl_row_t{ vec });
    }
    {
        table.rows.push_back(tbl_row_t{ tbl_rows{ tblstr{ "num samples" }, tblint{ ptr->getLenSamples() } } });
    }
    {
        table.rows.push_back(tbl_row_t{ tbl_rows{ tblstr{ "ticks start" }, tblint{ ptr->start() } } });
        table.rows.push_back(tbl_row_t{ tbl_rows{ tblstr{ "ticks end" }, tblint{ ptr->end() } } });
        table.rows.push_back(tbl_row_t{ tbl_rows{ tblstr{ "ticks length" }, tblint{ ptr->getLen() } } });
        table.rows.push_back(tbl_row_t{ tbl_rows{ tblstr{ "color" }, tblint{ ptr->rgb, "%08x" } } });
    }
#ifdef TODO_PROPERTIES_TABLE_CLIP_WAVEFORM_PROPERTIES
    {
        audioclip_texture_t waveform          = ptr->audio.waveformRef.waveform;
        gui_waveform_texture_ref& waveformRef = ptr->audio.waveformRef;

        //table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform", FONT_SIZE_TOOLTIP_BIG}, tblint{waveform.quality}}}});
        table.rows.push_back(tbl_row_t{ tbl_rows{ { tblstr{ "waveform samplesPerPx" }, tblfloat{ (float) waveform.samplesPerPx } } } });
        table.rows.push_back(tbl_row_t{ tbl_rows{ { tblstr{ "waveform pos" }, waveform.pos } } });
        table.rows.push_back(tbl_row_t{ tbl_rows{ { tblstr{ "waveform size" }, waveform.size } } });
        //table.rows.push_back(tbl_row_t{tbl_rows{{tblstr{"waveform startOffset"}, waveform.startOffset}}});
        table.rows.push_back(tbl_row_t{ tbl_rows{ { tblstr{ "waveform clipped" }, tblstr{ (waveform.clipped ? "yes" : "no") } } } });
        table.rows.push_back(tbl_row_t{ tbl_rows{ { tblstr{ "waveform quality" }, tblint{ waveform.quality } } } });
        table.rows.push_back(tbl_row_t{ tbl_rows{ { tblstr{ "waveform scaleX" }, tblfloat{ waveform.scaleX } } } });
        table.rows.push_back(tbl_row_t{ tbl_rows{ { tblstr{ "waveformRef atlasId" }, tblint{ waveformRef.atlasId } } } });
        table.rows.push_back(tbl_row_t{ tbl_rows{ { tblstr{ "waveformRef atlasEntryId" }, tblint{ waveformRef.atlasEntryId } } } });
    }
#endif
}

guictxtmenu_base* gui_audio_clip::getTooltip(AppCtrl* appctrl) {
    auto tooltip = new guitooltip<clip_t>(this->m_clip);
    return tooltip;
}

void gui_audio_clip::onIdle() {
}

void gui_audio_clip::onTick(AppCtrl* appctrl) {
}

void gui_clip::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
    view->dragSelectionBegin(this, evt);
}

void gui_clip::trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
    view->dragSelectionMove(this, evt);
}

void gui_clip::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
    view->dragSelectionRelease(this, evt);
    //!CLIP COULD BE DELETED AT THIS POINT
}

gui_track::gui_track(track_gui_entry_t* _entry, scaled_grid& _grid)
    : gui_track_content_base(_entry, _grid), automation(_entry, _grid, _entry->state.selectedAutomationCtr, _entry->state.selectedAutomationParam, subtrackIdx) {
    padding = 0;
}

void gui_track::updateVisibleTrackContents(project_globals_t& project, scaled_grid& grid) {
    automation.setData();
    automation.updateVisibleTrackContents(grid);
    std::vector<clip_t*> clips = m_track->getClips().getClips();
    for (clip_t* clip : clips) {
        auto* gui = DAW::createClipGui(this, m_trackentry, clip);
        dbgassert(gui);
        if (gui->parent != this) {
            add(gui);
        }
        gui->updatePosition(project, grid, size);
    }
    for (gui_track_subtrack* gui : m_trackentry->subtracks) {
        const bool throttleRefresh = m_trackentry->parentCtrl->isZooming();
        gui->updatePosition(project, grid, size, throttleRefresh);
    }
}

bool gui_track::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
            evt.requestFocus(this);
            return true;
        }
    }
    if (automation.mouseHitTest(mpos, evt)) {
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
            // scaled_grid& grid = m_trackentry->parentCtrl->getGrid();
            // tick_t tick       = grid.screenToTickSnap(mpos.x, SNAP_OFF);
            // if (m_trackentry->parentCtrl->getCursor().contains(this->m_trackentry->idx, tick)) {
                evt.requestFocus(this);
                return true;
            // }
        }
        // tracks need to always cancel further mouse tests for z-order to work in parent container
        return true;
    }
    return false;
}

void gui_track_subtrack::updateVisibleTrackContents(scaled_grid& grid) {
    guiTrAutomation.setData();
    guiTrAutomation.updateVisibleTrackContents(grid);
}

gui_track_automationlane::gui_track_automationlane(track_gui_entry_t* _entry, scaled_grid& _grid, automatable_t* _at, int32_t _param)
    : gui_track_subtrack(_entry, _grid, _at, _param) {
}

gui_track_subtrack::gui_track_subtrack(track_gui_entry_t* _entry, scaled_grid& _grid, automatable_t* _at, int32_t _param)
    : guictr_base(),
      m_grid(_grid),
      m_track(_entry->track),
      m_trackentry(_entry),
      guiTrAutomation(_entry, _grid, this->at, param, idx),
      at(_at),
      param(_param) {
    padding = 0;
}

class guictxtmenu_trackcontent : public guictxtmenu_track_editor {

public:
    //TODO make this take a safe reference to a track
    guictxtmenu_trackcontent(guitrack_editor* const _editor, track_gui_entry_t* const _trackentry)
        : guictxtmenu_track_editor(_editor, _trackentry, nullptr) {
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (guictxtmenu_track_editor::clickedElement(e, _id)) {
            return true;
        }
        return false;
    }
};

void gui_track_automationlane::handleRightClick(MouseEvent& evt) {
    auto trackEditor = guiParentType<guitrack_editor, gui_type::CTR_TYPE_TRACKS_EDITOR>(this->parent);
    if (!assert_expr(trackEditor)) {
        return;
    }
    parentCtrl->openContextMenu(new guictxtmenu_trackcontent(trackEditor, m_trackentry), evt.mousepos);
}
void gui_track_subtrack::handleRightClick(MouseEvent& evt) {
    auto trackEditor = guiParentType<guitrack_editor, gui_type::CTR_TYPE_TRACKS_EDITOR>(this->parent);
    if (!assert_expr(trackEditor)) {
        return;
    }
    parentCtrl->openContextMenu(new guictxtmenu_trackcontent(trackEditor, m_trackentry), evt.mousepos);
}
void gui_track::handleRightClick(MouseEvent& evt) {
    auto trackEditor = guiParentType<guitrack_editor, gui_type::CTR_TYPE_TRACKS_EDITOR>(this->parent);
    if (!assert_expr(trackEditor)) {
        return;
    }
    parentCtrl->openContextMenu(new guictxtmenu_trackcontent(trackEditor, m_trackentry), evt.mousepos);
}
void guitrack_editor::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_trackcontent(this, nullptr), evt.mousepos);
}

void gui_track_subtrack::renderMixerInfo(NVGcontext* vg, ivec2 pos, ivec2 size) {
    DAW::Cursor& cursor = m_trackentry->parentCtrl->getCursor();
    String curvalue     = "UNDEF";
    String target       = "<NULL>";
    automatable_t* ctr  = at;
    if (ctr) {
        target      = StringFormat("%s %12zX", StringAsCStr(ctr->getAutomatableName()), reinterpret_cast<uint64_t>(ctr));
        int32_t paramIdx = param;
        if (paramIdx >= 0) {
            auto automation = ctr->getRegisteredAutomation(paramIdx);
            if (automation) {
                curvalue = StringFormat("%s (%d) %f", StringAsCStr(ctr->getParamName(paramIdx)), paramIdx, automation->getValueAt(cursor.cursorPos));
            } else {
                curvalue = StringFormat("%s (%d) UNDEF", StringAsCStr(ctr->getParamName(paramIdx)), paramIdx);
            }
        } else {
            curvalue = StringFormat("<NULL> %d", paramIdx);
        }
    }
    const int htt         = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    const int fontSize    = htt;
    renderTextLabel(vg, 
        vec2(0, htt * 0.5f) + vec2(INSET_TITLE),
        vec2(size.x - INSET_TITLE, htt),
        target,
        theme, fontSize, theme->getColor(GuiColor::COL_WHITE), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    renderTextLabel(vg, 
        vec2(0, htt * 1.5f) + vec2(INSET_TITLE),
        vec2(size.x - INSET_TITLE, htt),
        curvalue,
        theme, fontSize, theme->getColor(GuiColor::COL_WHITE), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
}

void gui_track::renderTrackFolded(NVGcontext* vg) {
    auto ctrTracks = m_trackentry->parent;
    if (!m_track->children.empty()) {
        std::vector<track_gui_entry_t*> children;
        std::vector<track_t*> queue;
        queue.push_back(m_track);
        while (!queue.empty()) {
            track_t* const tr = queue.back();
            queue.pop_back();
            for (auto& child : tr->children) {
                track_gui_entry_t* trEntryChild = nullptr;
                if (ctrTracks->getTrackEntry(child, &trEntryChild)) {
                    if (!trEntryChild->clipsGuis.empty()) {
                        children.push_back(trEntryChild);
                    }
                }
                queue.push_back(child);
            }
        }
        if (children.empty())
            return;
        auto entryHeight = float(size.y) / children.size();
        vec2 size = vec2(this->size.x, entryHeight);
        vec2 pos = vec2(0, 0);
        NVGpaint paint{};
        paint.image     = -1;
        paint.customPar = 1;
        for (auto& child : children) {
            for (auto& entry : child->clipsGuis) {
                if (entry.first && entry.second) {
                    clip_t* const cl = entry.first;
                    vec2 clipPos{};
                    vec2 clipSize = size;
                    bool bCulled = !getClipPositionFloat(ctrTracks->m_grid, size, cl, clipPos, clipSize, 0.0);
                    if (!bCulled) {
                        NVGcolor color = rgbToNvg(cl->rgb);
                        if (!cl->enabled) {
                            color = rgbToNvg(0x333333);
                        }
                        paint.innerColor = color;
                        if (clipPos.x < -clipSize.x-4) {
                            continue;
                        }
                        if (clipPos.x > size.x+4) {
                            continue;
                        }
                        clipPos += pos;

                        nvgBatchedRect(vg, clipPos.x, clipPos.y, clipSize.x, clipSize.y);
                        nvgFillPaint(vg, paint);
                        nvgBatchedRender(vg);
                        int inset = 2;
                        if (clipSize.y > inset && clipSize.x > inset) {
                            clipPos += vec2(inset, inset);
                            clipSize -= vec2(inset * 2, inset * 2);
                            color = { 0.1f, 0.1f, 0.1f, 0.42f };
                            paint.innerColor = color;
                            nvgBatchedRect(vg, clipPos.x, clipPos.y, clipSize.x, clipSize.y);
                            nvgFillPaint(vg, paint);
                            nvgBatchedRender(vg);
                        }
                    }
                }
            }
            pos.y += entryHeight;
        }
    }
}

void gui_track::render(NVGcontext* vg) {
    if (!isRenderableSizeAndContext(vg))
        return;
    if (dawCtrl->getSelectedTrack() == m_track) {
        nvgBeginPath(vg);
        nvgRect(vg, pos.x, pos.y, size.x, size.y);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_SELECTEDTRACK));
        nvgFill(vg);
    }
    nvgSave(vg);
    if (setScissorTransform(vg)) {
        if (!m_track->children.empty()) {
            renderTrackFolded(vg);
        }
        renderTrack(vg);
    }
    nvgRestore(vg);
    nvgSave(vg);
    automation.render(vg);
    nvgRestore(vg);
    if (!automation.isRenderingLane()) {
        return;
    }
}

void gui_track::renderTrack(NVGcontext* vg) {
    for (auto& entry : m_trackentry->clipsGuis) {
        if (entry.second) {
            entry.second->render(vg);
        }
    }
}

void gui_track::renderDebugPass(NVGcontext* vg) {
    ivec2 posInset  = getPosContent();
    ivec2 sizeInset = getSizeContent();

    if (sizeInset.y <= 0 || sizeInset.x <= 0) {
        return;
    }

    nvgSave(vg);
    nvgTranslate(vg, posInset.x, posInset.y);
    for (auto& entry : m_trackentry->clipsGuis) {
        if (entry.second) {
            entry.second->renderDebugPass(vg);
        }
    }
    nvgRestore(vg);
}

bool gui_track_subtrack::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
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
            scaled_grid& grid = m_trackentry->content->getGrid();
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
