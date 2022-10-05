#include <glm/geometric.hpp>
#include <memory>
#include <numeric>
#include <vector>

#include "assert_dbg.h"
#include "event.h"
#include "gui/controls/button.h"
#include "gui/dropdown/dropdown.h"
#include "guicolors.h"
#include "gui/table/table.h"
#include "gui/tooltip/tooltip.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/contextmenu/contextmenu_grid.h"
#include "gui/plugin/pluginctr.h"

#include "basectrl.h"
#include "host/mainctrl.h"

#include "keyboard.h"
#include "track.h"
#include "trackautomation.h"
#include "track_impl.h"

#include "samplerate.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "audiocache.h"
#include "wave/waveform_render.h"
#include "wave/waveform_render_impl.h"
#include "host/host_pluginmanager.h"

struct track_gui_entry_t;

constexpr int32_t CLIPPING_STEP_PX = 512;
constexpr int32_t MARGIN_CLIPPING_PX = 32;


bool getClipPosition(scaled_grid& grid, const ivec2& scissorSize, const clip_t* cl, ivec2& pos, ivec2& size, tick_t offset) {
    tick_t tickBegin  = cl->time + offset;
    tick_t tickEnd    = cl->time + offset + cl->getLen();
    double tickBeginX = grid.tickToScreenD(tickBegin);
    double tickEndX   = grid.tickToScreenD(tickEnd);
    if (tickEndX < -MARGIN_CLIPPING_PX || tickBeginX > scissorSize.x + MARGIN_CLIPPING_PX) {
        return false;
    }
    double width = tickEndX - tickBeginX;

    dbgassert(FitsTypeRange<int32_t>(tickBeginX));
    dbgassert(FitsTypeRange<int32_t>(tickEndX));

    int32_t tickBeginPx = math::rounddS32(tickBeginX);
    int32_t widthPx     = math::rounddS32(width);

    pos  = ivec2(tickBeginPx, INSET_TRACK_CONTENT);
    size = math::maxvec2(ivec2(widthPx, size.y - INSET_TRACK_CONTENT * 2), ivec2(0));

    //dbgassert(size.x > 0 && size.y > 0);

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

        gui_waveform_texture_ref* ref = getWaveformTextureRef();
        auto file = dawCtrl->getDaw()->getAudioCache()->get(m_clip->audio.id);
        renderAudioClip(vg, dawCtrl->getWaveformRenderer(), theme, m_track, m_clip, file, ref, pos, size, posClipped, sizeClipped);
        nvgBeginPath(vg);
        nvgRect(vg, posClipped.x, posClipped.y, sizeClipped.x, sizeClipped.y);
        nvgFillColor(vg, rgbaToNvg(0x7Fff00ff));
        nvgFill(vg);
    }
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
    }
}

void gui_midi_clip::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_clip(dawCtrl, this), evt.mousepos);
}

void gui_audio_clip::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_clip(dawCtrl, this), evt.mousepos);
}

void gui_audio_clip::updateClipRenderCache(NVGcontext* vg) {
}

void gui_audio_clip::updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) {
    size   = this->parent->size;
    culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);

    audiofile_t* audio = dawCtrl->getDaw()->getAudioCache()->get(m_clip->audio.id);

    if (culled || !audio) {
        releaseWaveformTexture();
        return;
    }

    dbgassert(size.x > 0);

    const auto HEIGHT_CLIP_TITLE = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    ivec2 shrink = ivec2(0, (HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT * 2));
    ivec2 sizeClipped = size - shrink;
    ivec2 posClipped = pos + shrink;

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

void rendered_audio_clip_t::updateClipPrerender(NVGcontext* vg, audiofile_t* audio, bool culled) {
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
    updateClipPrerender(vg, audio, culled);
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
    : gui_track_content_base(_entry), automation(_entry, _grid, _entry->state.selectedAutomationCtr, _entry->state.selectedAutomationParam, subtrackIdx) {
    padding = 0;
}

gui_track* createTrackGui(track_gui_entry_t* _entry, scaled_grid& grid) {
    auto* const guitrack = new gui_track(_entry, grid);
    guitrack->setZOrder(TRACKTYPE_TO_CTR(_entry->track->type) == TRACK_CTR_MIDIAUDIO ? 0 : 1);
    return guitrack;
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

void gui_track::updateVisibleTrackContents(project_globals_t& project, scaled_grid& grid) {
    automation.setData();
    automation.updateVisibleTrackContents(grid);
    std::vector<clip_t*> clips = m_track->getMidi().getClips();
    for (clip_t* clip : clips) {
        auto* gui = createClipGui(this, m_trackentry, clip);
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
            if (gui->mouseHitTest(localMouse, evt)) {
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
    guictxtmenu_trackcontent(DawCtrl* const _dawCtrl, track_gui_entry_t* const _trackentry)
        : guictxtmenu_track_editor(_dawCtrl, _trackentry, nullptr) {
    }
    bool clickedElement(ctxtmenu_entry* e, int _id) override {
        if (guictxtmenu_track_editor::clickedElement(e, _id)) {
            return true;
        }
        scaled_grid& grid = dawCtrl->getGrid();
        if (_id == 110 + 9) {// OFF
            grid.grid_dens.enabled = false;
        } else if (_id >= 110) {
            grid.grid_dens.enabled   = true;
            grid.grid_dens.fixedBars = _id - 110;
            grid.grid_dens.isfixed   = true;
        } else {
            grid.grid_dens.enabled        = true;
            grid.grid_dens.dynamicDensity = _id - 100;
            grid.grid_dens.isfixed        = false;
        }
        dawCtrl->getDaw()->updateVisibleTrackContents();
        closeContextMenu();
        return true;
    }
};

void gui_track_automationlane::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_trackcontent(m_trackentry->parentCtrl, m_trackentry), evt.mousepos);
}
void gui_track_subtrack::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_trackcontent(m_trackentry->parentCtrl, m_trackentry), evt.mousepos);
}
void gui_track::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_trackcontent(m_trackentry->parentCtrl, m_trackentry), evt.mousepos);
}
void guitrack_editor::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_trackcontent(dawCtrl, nullptr), evt.mousepos);
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
