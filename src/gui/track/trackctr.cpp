#include <nanovg.h>
#include "assert_dbg.h"
#include "guicolors.h"
#include "str_util.h"
#include "tls.h"
#include "trackctr.h"
#include "math/seq_math.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "exceptions.h"
#include "theme.h"
#include "automation.h"
#include "track.h"
#include "trackcontent.h"
#include "trackcontrols.h"
#include "track.h"
#include "track_impl.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "logging.h"
#include "subtrack.h"
#include "appconfig.h"

#include "gui/contextmenu/contextmenu_daw.h"

void guitrack_mixers::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    ivec2 cs = getSizeContent();
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, cs.x, cs.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
    nvgFill(vg);
    for (track_t* g : project.tracksBottom) {

        track_gui_entry_t* entry;
        if (!iGuiMgr.getPointerEntry(g, &entry)) {
            dbgassert(0);
            continue;
        }
        dbgassert(entry->mixer->isVisible() == iGuiMgr.isVisible(entry));
        if (entry->mixer->isVisible()) {
            nvgSave(vg);
            entry->mixer->renderGroupHandle(vg);
            entry->mixer->render(vg);
            nvgRestore(vg);
        }
    }
    int ySplit = getPosYFirstReturnTrack(iGuiMgr.getTracksVisibleFlat());
    if (ySplit > 0) {
        nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
        for (track_gui_entry_t* entry : iGuiMgr.getTracksVisibleFlat()) {
            dbgassert(entry->mixer->isVisible() == iGuiMgr.isVisible(entry));
            auto& mixer = entry->mixer;
            if (mixer->isVisible() && mixer->pos.y < ySplit && mixer->bottom() > 0) {
                nvgSave(vg);
                mixer->renderGroupHandle(vg);
                mixer->render(vg);
                nvgRestore(vg);
            }
        }
    }
}
void guitrack_mixers::addTrackEntry(track_gui_entry_t& e) {
    this->add(e.mixer);
}
void guitrack_mixers::removeTrackEntry(track_gui_entry_t& e) {
    this->remove(e.mixer);
}

void drawSeperator(NVGcontext* vg, const guitheme_t* theme, int32_t seperatorY, const ivec2& cs) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, seperatorY);
    nvgLineTo(vg, cs.x, seperatorY);
    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
    nvgStrokeWidth(vg, TRACK_HEIGHT_SPACING);
    nvgStroke(vg);
}
int32_t guictr_tracks::getTrackTotalHeight(track_gui_entry_t* e) {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

    int32_t trH = e->layout.hideTrack ? 1 : e->layout.height;
    int32_t totalHeight = trH * TRACK_HEIGHT_STEP;

    if (!(e->layout.hideTrack || e->layout.hideSubtracks)) {
        for (auto t2 : e->subtracks) {
            totalHeight += t2->height * TRACK_HEIGHT_STEP + TRACK_HEIGHT_SPACING;
        }
    }
    return totalHeight;
}
int32_t guictr_tracks::setTrackPosition(track_gui_entry_t* e, int32_t y, bool isBottom) {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

    int32_t childTrackInsetX = e->track->getChildLvl() * 8;

    ivec2& cntPos    = e->content->pos;
    ivec2& mxrPos    = e->mixer->pos;
    cntPos           = ivec2(0, y);
    mxrPos           = ivec2(childTrackInsetX, y);
    int32_t trH      = e->layout.hideTrack ? 1 : e->layout.height;
    e->content->size = ivec2(trackView.size.x, trH * TRACK_HEIGHT_STEP);
    int32_t x2       = e->content->left();
    int32_t y2       = e->content->bottom();

    if (!(e->layout.hideTrack || e->layout.hideSubtracks)) {
        for (auto t2 : e->subtracks) {
            int trackheight2 = t2->height * TRACK_HEIGHT_STEP;

            t2->pos  = ivec2(x2, y2);
            t2->size = ivec2(e->content->size.x, trackheight2);

            y2 = t2->bottom() + TRACK_HEIGHT_SPACING;
        }
    } else {
        for (auto t2 : e->subtracks) {
            t2->pos  = ivec2(x2, y2);
            t2->size = ivec2(0, 0);
        }
    }

    int32_t totalHeight = y2 - y;
    e->mixer->size = ivec2(trackControls.size.x - childTrackInsetX, totalHeight);

    if (isBottom) {
        cntPos.y -= totalHeight;
        mxrPos.y -= totalHeight;
        for (auto t2 : e->subtracks) {
            t2->pos.y -= totalHeight;
        }
    }
    e->content->positionChanged();
    for (auto t2 : e->subtracks) {
        t2->positionChanged();
    }
    return totalHeight;
}

void guictr_tracks::showAutomationLane(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx) {
    entry->state.selectedAutomationCtr   = at;
    entry->state.selectedAutomationParam = paramIdx;
}

void guictr_tracks::addSubTrack(track_gui_entry_t* entry, gui_track_subtrack* subtrack, bool insertFront) {
    trackView.addSubtrack(entry, subtrack, insertFront);
    entry->mixer->addSubtrackMixer(entry, subtrack);
}
void guictr_tracks::removeSubtrack(track_gui_entry_t* entry, gui_track_subtrack* subtrack) {
    trackView.removeSubtrack(entry, subtrack);
    entry->mixer->removeSubtrackMixer(subtrack);
}

gui_track_automationlane* guictr_tracks::addAutomationLane(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx, bool insertFront) {
    auto* al = new gui_track_automationlane(entry, grid, at, paramIdx);
    addSubTrack(entry, al, insertFront);
    return al;
}
void guictr_tracks::removeAutomationLane(gui_track_automationlane* al) {
    track_gui_entry_t* entry;
    always_assert(guiMgr.getTrackEntry(al->m_track, &entry));
    entry->mixer->removeSubtrackMixer(al);
    trackView.removeSubtrack(entry, al);
}
void guictr_tracks::removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx) {
    entry->mixer->removeAllAutomationLanes(at, paramIdx);
    trackView.removeAllAutomationLanes(entry, at, paramIdx);
}
void guictr_tracks::removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at) {
    entry->mixer->removeAllAutomationLanes(at);
    trackView.removeAllAutomationLanes(entry, at);
}
void guictr_tracks::removeAllSubtracks(track_gui_entry_t* entry) {
    entry->mixer->removeAllSubtracks();
    trackView.removeAllSubtracks(entry);
}
void guictr_tracks::resetView() {
    trackView.m_resizePreModifyState.reset();
    trackView.m_clipboard.reset();
    trackView.action.clipboard.reset();
    trackView.iGuiMgr.reset();
}

void loadSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, const track_layout_snapshot_t& snapshot);
void loadTrackLayoutSettings(guictr_tracks* guiTracks, track_gui_entry_t* entry, const tracklayout_settings_t& settings);

void loadTrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, const track_layout_snapshot_t& snapshot) {
    entry->subtracks.clear();
    bool hide = entry->layout.hideSubtracks || entry->layout.hideTrack;
    if (hide) {
        entry->state.layoutSaved = snapshot;
        entry->state.wasInHide   = true;
    } else {
        entry->state.wasInHide = false;
        loadTrackLayoutSettings(guiTracks, entry, snapshot.layout);
        loadSubtrackLayout(guiTracks, entry, snapshot);
        entry->state.layoutSaved = track_layout_snapshot_t();
    }
}

void guictr_tracks::loadTrackLayouts(trackcontainer_snapshot_t& in) {
    for (track_snapshot_t& trackStatic : in.tracks) {
        dbgassert(trackStatic.trackLoaded);
        auto it = trackStatic.layouts.find(globalIndex);
        if (it != trackStatic.layouts.end()) {
            track_layout_snapshot_t& layout = it->second;
            track_gui_entry_t* entry{};
            always_assert(guiMgr.getTrackEntry(trackStatic.trackLoaded, &entry));
            loadTrackLayout(this, entry, layout);
        }
        trackStatic.trackLoaded = nullptr;
    }
}

void guictr_tracks::scrollOffsetChanged(int dir, float offset) {
    int32_t scrOffset = math::max(0.0f, offset * (contentHeight - contentViewSize));

    int y = TRACK_HEIGHT_SPACING - scrOffset;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            int32_t h = setTrackPosition(entry, y, false);
            y += h + TRACK_HEIGHT_SPACING;
        } else {
            dbgassert(0);
        }
    }

    // updateVisibleTrackContents();
}

void guictr_tracks::scrollTo(guibase* g) {
    int32_t scrOffset = math::max(0.0f, scrollbar.scrollOffset * (contentHeight - contentViewSize));

    int32_t y = g->pos.y;
    scrollbar.scrollVisible(y + scrOffset, g->size.y);
}

void guictr_tracks::updateVisibleTracks() {
    guiMgr.updateVisibleTracks(project.trackList);

    track_gui_vector_td& tracks = guiMgr.tracksVisibleFlat;
    for (track_t* tr : project.trackList) {
        track_gui_entry_t* entry;
        if (!(guiMgr.getPointerEntry(tr, &entry))) {
            continue;
        }
        if (!assert_expr(entry->content)) {
            continue;
        }
        const bool bVisible = STL_CONTAINS(tracks, entry);
        entry->content->setVisible(bVisible);
        entry->mixer->setVisible(bVisible);
        if (bVisible) {
            entry->content->updateVisibleTrackContents(projectGlobals, grid);
            for (gui_track_subtrack* au : entry->subtracks) {
                au->updateVisibleTrackContents(grid);
            }
        }
    }
}
void guictr_tracks::layoutVisibleTracks() {
    track_gui_vector_td& tracks = guiMgr.tracksVisibleFlat;
    for (track_gui_entry_t* entry : tracks) {
        entry->content->updateVisibleTrackContents(projectGlobals, grid);
        for (gui_track_subtrack* au : entry->subtracks) {
            au->updateVisibleTrackContents(grid);
        }
    }
}

void guictr_tracks::layout() {
    const int32_t trackControlsWidth = theme->get(GuiConstant::CONST_TRACK_CONTROLS_WIDTH);

    bool trackCtrlsLeft = true;
    int scrollW         = gui_scrollbar::defaultW;

    ivec2 cs       = getSizeContent();
    scrollbar.pos  = ivec2(cs.x - scrollW, 0);
    scrollbar.size = ivec2(scrollW, cs.y);

    trackTimeline.pos  = ivec2(trackCtrlsLeft ? trackControlsWidth : 0, 0);
    trackTimeline.pos  = ivec2(trackCtrlsLeft ? trackControlsWidth : 0, 0);
    trackTimeline.size = ivec2(cs.x - trackControlsWidth, 32);
    loophandles.pos    = ivec2(trackTimeline.left(), trackTimeline.bottom());
    loophandles.size   = ivec2(trackTimeline.size.x, heightTimelineControls);
    trackTopLeft.pos   = ivec2(trackCtrlsLeft ? 0 : cs.x - trackControlsWidth, 0);
    trackView.pos      = ivec2(trackCtrlsLeft ? trackControlsWidth : 0, loophandles.bottom());
    trackControls.pos  = ivec2(trackCtrlsLeft ? 0 : cs.x - trackControlsWidth, loophandles.bottom());
    trackView.size     = ivec2(cs.x - trackControlsWidth, cs.y - loophandles.bottom());
    trackTopLeft.size  = ivec2(trackControlsWidth, loophandles.bottom());
    trackControls.size = ivec2(trackControlsWidth, trackView.size.y);

    loophandles.clipViewSize = ivec2(trackView.size.x, trackView.size.y + loophandles.size.y);

    ivec2 csTrackView = trackView.getSizeContent();

    // Calculate the combined height of all top tracks
    int32_t allTracksHeight = 0;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            allTracksHeight += getTrackTotalHeight(entry);
            allTracksHeight += TRACK_HEIGHT_SPACING;
        }
    }

    // Calculate the y position of the first return
    int32_t yPosFirstReturn = csTrackView.y - TRACK_HEIGHT_SPACING;
    auto itMastersTracks    = guiMgr.trackEntriesBottom.rbegin();
    auto itMastersEnd       = guiMgr.trackEntriesBottom.rend();
    while (itMastersTracks != itMastersEnd) {
        auto& entry = *itMastersTracks;
        if (guiMgr.isVisible(entry)) {
            yPosFirstReturn -= getTrackTotalHeight(entry);
            yPosFirstReturn -= TRACK_HEIGHT_SPACING;
        }
        itMastersTracks++;
    }
    contentHeight   = allTracksHeight;
    contentViewSize = yPosFirstReturn;
    //if (contentHeight >= contentViewSize) {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    contentHeight += TRACK_HEIGHT_STEP * 4;
    //}

    int32_t scrOffset = math::max(0.0f, getScrollOffset() * (contentHeight - contentViewSize));

    int y = TRACK_HEIGHT_SPACING - scrOffset;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            int32_t h = setTrackPosition(entry, y, false);
            y += h + TRACK_HEIGHT_SPACING;
        } else {
            dbgassert(0);
        }
    }


    y = csTrackView.y - TRACK_HEIGHT_SPACING;

    itMastersTracks = guiMgr.trackEntriesBottom.rbegin();
    itMastersEnd    = guiMgr.trackEntriesBottom.rend();
    while (itMastersTracks != itMastersEnd) {
        auto& entry = *itMastersTracks;
        if (guiMgr.isVisible(entry)) {
            dbgassert(entry->content);
            int32_t h = setTrackPosition(entry, y, true);
            y -= h;
            y -= TRACK_HEIGHT_SPACING;
        } else {
            dbgassert(0);
        }


        itMastersTracks++;
    }

    for (guibase* gui : guis) {
        gui->layout();
    }
}

void horizontalLineAt(guictr_base* gui, NVGcontext* vg, ivec2 posHL) {
    nvgLineCap(vg, NVGlineCap::NVG_ROUND);
    nvgBeginPath(vg);
    nvgMoveTo(vg, 4, posHL.y);
    int32_t width = gui->getSizeContent().x;
    nvgLineTo(vg, width - 4, posHL.y);
    nvgStrokeColor(vg, gui->theme->getColor(GuiColor::COL_DRAGDROPMOVE_HIGHLIGHT));
    nvgStrokeWidth(vg, 4.0);
    nvgStroke(vg);
    nvgLineCap(vg, NVGlineCap::NVG_BUTT);
}

void guictr_tracks::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    ivec2 cs = getSizeContent();
    ivec2 cp = getPosContent();
    if (cs.y <= 0 || cs.x <= 0) {
        return;
    }

    auto& dawtls = daw_tls::getTls();

    hires_timer_t timer;
    nvgIntersectScissor(vg, cp.x, cp.y, cs.x, cs.y);
    nvgTranslate(vg, cp.x, cp.y);

    nvgSave(vg);
    timer.reset();
    trackView.render(vg);
    dawtls.runtime->renderStats.timeRenderEditor = timer.getTime();
    nvgRestore(vg);

    nvgSave(vg);
    trackTopLeft.render(vg);
    nvgRestore(vg);
    nvgSave(vg);
    timer.reset();
    trackControls.render(vg);
    dawtls.runtime->renderStats.timeRenderTrackControls = timer.getTime();
    nvgRestore(vg);


    nvgSave(vg);
    trackTimeline.render(vg);
    nvgRestore(vg);

    nvgSave(vg);
    dragdrop_target_indicator_t& dragDropTarget = dawCtrl->getDragDropTarget();
    nvgTranslate(vg, 0, trackView.top());
    int ySplit = getPosYFirstReturnTrack(guiMgr.tracksVisibleFlat);
    if (ySplit > 0) {
        nvgSave(vg);
        nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
        for (track_t* t : project.trackMidiAudioCtr) {
            track_gui_entry_t* entry;
            if (guiMgr.getTrackEntry(t, &entry) && guiMgr.isVisible(entry)) {
                drawSeperator(vg, theme, entry->mixer->bottom() + TRACK_HEIGHT_SPACING_HALF, cs);
            }
        }
        nvgRestore(vg);
    }
    if (!project.tracksBottom.empty() && (ySplit <= 0 || trackView.size.y > ySplit)) {
        if (ySplit > 0) {
            nvgIntersectScissor(vg, 0, ySplit, cs.x, trackView.size.y - ySplit);
        } else {
            nvgIntersectScissor(vg, 0, 0, cs.x, trackView.size.y);
        }
        for (track_t* t : project.tracksBottom) {
            track_gui_entry_t* entry;
            if (guiMgr.getTrackEntry(t, &entry) && guiMgr.isVisible(entry)) {
                drawSeperator(vg, theme, entry->mixer->top() - TRACK_HEIGHT_SPACING_HALF, cs);
            }
        }
    }
    nvgRestore(vg);
    if (dragDropTarget.src == this && dragDropTarget.dst) {
        nvgSave(vg);
        nvgTranslate(vg, 0, trackView.top());
        int n       = this->theme->get(GuiConstant::CONST_GUI_INSET_WIDGET_BG);
        auto bgPos  = ivec2(n);
        auto bgSize = this->getSizeContent() - ivec2(n * 2);
        if (bgSize.x > 0 && bgSize.y > 0) {
            nvgGlobalAlpha(vg, 0.5f);
            nvgBeginPath(vg);
            nvgRect(vg, bgPos.x, bgPos.y, bgSize.x, bgSize.y);
            if (dragdrop_target_indicator_t::target_area == dragDropTarget.type) {
                nvgPathWinding(vg, NVGwinding::NVG_CW);
                nvgRect(vg, dragDropTarget.dst->pos.x, dragDropTarget.dst->pos.y, dragDropTarget.dst->size.x, dragDropTarget.dst->size.y);
                nvgPathWinding(vg, NVGwinding::NVG_CCW);
            }
            nvgFillColor(vg, theme->getColor(getBackgroundColor()));
            nvgFill(vg);
            nvgGlobalAlpha(vg, 1.0f);
        }
        const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        ivec2 indicatorPos    = dragDropTarget.targetPos;
        horizontalLineAt(this, vg, indicatorPos);

        int fontScale = titleHeight;
        auto str = dragDropTarget.dst->getLabel() + StringFormat("[%d]", dragDropTarget.slotIdx);
        renderCenteredMultilineText(vg, theme, str, fontScale, getLabelColor(), indicatorPos, ivec2(titleHeight * 30, titleHeight * 2));

        nvgRestore(vg);
    }

    nvgBeginPath(vg);
    nvgMoveTo(vg, trackControls.left(), trackControls.top());
    nvgLineTo(vg, trackControls.left(), trackControls.bottom());
    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
    nvgStrokeWidth(vg, 3);
    nvgStroke(vg);


    nvgSave(vg);
    loophandles.render(vg);
    nvgRestore(vg);


    nvgSave(vg);
    scrollbar.render(vg);
    nvgRestore(vg);

    if (dawtls.runtime->enableClipRendererDebugLayer) {
        trackView.renderDebugPass(vg);
    }

    if (trackView.size.x > 0) {
        nvgIntersectScissor(vg, trackView.pos.x, 0, trackView.size.x, cs.y);
        nvgTranslate(vg, trackView.pos.x, 0);
        tick_t pos = dawCtrl->getDaw()->getPlaybackPos();

        float playBackX = (float) grid.tickToScreenD(pos);
        if (playBackX > -4.0f && playBackX < cs.x + 4.0f) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, playBackX, 0);
            nvgLineTo(vg, playBackX, cs.y);
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLAYHEAD_OUTLINE));
            nvgStrokeWidth(vg, 3);
            nvgStroke(vg);
            nvgBeginPath(vg);
            nvgMoveTo(vg, playBackX, 0);
            nvgLineTo(vg, playBackX, cs.y);
            nvgStrokeColor(vg, theme->getColor(GuiColor::COL_PLAYHEAD));
            nvgStrokeWidth(vg, 1);
            nvgStroke(vg);
        }
    }
}
void guitrack_editor::addSubtrack(track_gui_entry_t* entry, gui_track_subtrack* al, bool insertFront) {

    if (insertFront) {
        entry->subtracks.insert(entry->subtracks.begin(), al);
    } else {
        entry->subtracks.push_back(al);
    }
    int32_t idx = 0;
    for (auto subTr : entry->subtracks) {
        subTr->idx = idx++;
    }
    al->setZOrder(entry->track->type >= TRACK_TYPE_MIDI ? 0 : 1);
    add(al);
}

void guitrack_editor::removeAllSubtracks(track_gui_entry_t* entry) {
    auto& atLanes = entry->subtracks;
    for (auto at : atLanes) {
        remove(at);
        delete at;
    }
    atLanes.clear();
}
void guitrack_editor::removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at) {
    removeAllAutomationLanes(entry, at, -1);
}
void guitrack_editor::removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx) {

    auto& atLanes = entry->subtracks;
    auto it       = std::remove_if(atLanes.begin(), atLanes.end(), [this, at, paramIdx](gui_track_subtrack* al) {
        if (al->subtrackType() != gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
            return false;
        }
        if ((!at || al->at == at) && (paramIdx < 0 || al->param == paramIdx)) {
            remove(al);
            delete al;
            return true;
        }
        return false;
          });
    atLanes.erase(it, atLanes.end());

    int32_t idx = 0;
    for (auto subTr : atLanes) {
        subTr->idx = idx++;
    }
}
void guitrack_editor::removeSubtrack(track_gui_entry_t* entry, gui_track_subtrack* al) {
    dbgassert(al);
    remove(al);
    auto& atLanes = entry->subtracks;
    auto it       = std::find(atLanes.begin(), atLanes.end(), al);
    dbgassert(it != atLanes.end());
    atLanes.erase(it);
    delete al;
    int32_t idx = 0;
    for (auto subTr : atLanes) {
        subTr->idx = idx++;
    }
}

bool guitrack_editor::mouseHitTest(ivec2 v, MouseHitEvt& evt) {
    if (this->contains(v)) {
        // if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
        //     evt.requestFocus(this);
        //     return true;
        // }
        if (evt.type == MOUSE_DRAGDROP_CLIP) {
            evt.requestFocus(this);
            return true;
        }
        ivec2 localMouse = this->toContainerSpace(v);
        for (guibase* gui : guis) {
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                if (!evt.getGuiHit()) 
                    break;
                return true;
            }
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}

void guitrack_editor::addTrackEntry(track_gui_entry_t& e) {

    add(e.content);

    //TODO: sort and render guis by track->idx
//#ifndef NDEBUG
//    for (guibase* child : guis) {
//        gui_track* t = dynamic_cast<gui_track*>(child);
//        dbgassert(t);
//    }
//    int idx = 0;
//    for (guibase* child : guis) {
//        gui_track* t = dynamic_cast<gui_track*>(child);
//        log_printf("idx %d = %s\n", idx, StringAsCStr(t->m_track->name));
//        idx++;
//    }
//#endif
}
bool track_gui_manager_t::getTrackEntry(const track_t* t, track_gui_entry_t** out) {
    return getPointerEntry(t, out);
}
bool track_gui_manager_t::getPointerEntry(const track_t* t, track_gui_entry_t** out) {
    *out    = nullptr;
    auto it = std::find_if(entries.begin(), entries.end(), [t](track_gui_entry_t* entry) {
        return entry->track == t;
    });
    if (it != entries.end()) {
        track_gui_entry_t* pEntry = *it;
        *out = pEntry;
        return true;
    }
    return false;
}
bool track_gui_manager_t::getTrackEntryCopy(const track_t* t, track_gui_entry_t& out) {
    auto it = std::find_if(entries.begin(), entries.end(), [t](track_gui_entry_t* entry) {
        return entry->track == t;
    });
    if (it != entries.end()) {
        track_gui_entry_t* pEntry = *it;
        out = *pEntry;
        return true;
    }
    return false;
}
void guitrack_editor::removeTrackEntry(track_gui_entry_t& entry) {
    if (entry.content) {
        entry.content->destroyGuis();
        remove(entry.content);
        dbgassert(entry.clipsGuis.empty());
    } else {
        dbgassert(0);
    }
    if (!entry.subtracks.empty()) {
        for (auto str : entry.subtracks) {
            remove(str);
            delete str;
        }
        entry.subtracks.clear();
    }
}
void guitrack_editor::layout() {
    for (guibase* gui : guis) {
        gui->layout();
    }
}

void guictr_tracks::removeAllTracks() {
    track_gui_vector_td tracksCopy = guiMgr.getTracksVisibleFlat();
    for (auto* entry : tracksCopy) {
        removeTrack(entry->track, FLG_TRK_CHANGE_LOAD);
    }
}
void guictr_tracks::removeTrack(track_t* track, int flags) {
    track_gui_entry_t* entry = nullptr;
    if (!guiMgr.getPointerEntry(track, &entry)) {
        log_printf("attempt to double remove track from container\n");
        return;
    }
    dbgassert(track->audio);
    removeAllSubtracks(entry);
    trackControls.removeTrackEntry(*entry);
    trackView.removeTrackEntry(*entry);
    removeEntry(track->audio->guiInstances, entry);
    dbgassert(entry->content);
    dbgassert(entry->mixer);
    delete entry->content;
    delete entry->mixer;
    entry->content = nullptr;
    entry->mixer   = nullptr;
    guiMgr.removeTrack(*entry);// does delete entry;
                               //trackView.addTrack(entry.content);
}

void guictr_tracks::addTrack(track_t* track, int flags) {
    dbgassert(track->audio);
    auto* entry = new track_gui_entry_t{};

    entry->parentCtrl = this->dawCtrl;
    entry->track      = track;
    entry->parent     = this;
    entry->mixer      = createTrackGuiMixer(entry);
    entry->content    = createTrackGui(entry, grid);

    guiMgr.addTrack(entry);
    trackControls.addTrackEntry(*entry);
    trackView.addTrackEntry(*entry);
    //track->content = entry->content;
    track->audio->guiInstances.push_back(entry);
    //TODO: restore subtracks
    if (!(flags & FLG_TRK_CHANGE_LOAD)) {
        updateVisibleTracks();
        layout();
    }
}
void guitrack_mixers::handleRightClick(MouseEvent& evt) {
    parentCtrl->openContextMenu(new guictxtmenu_notrack(), evt.mousepos);
}

void getTrackGuiYBounds(const track_gui_entry_t* track, ivec2& topBottom) {
    auto minVec = math::minvec2(track->mixer->getLeftTop(), track->content->getLeftTop());
    auto maxVec = math::maxvec2(track->mixer->getRightBottom(), track->content->getRightBottom());
    for (auto subtrack : track->subtracks) {
        maxVec = math::maxvec2(maxVec, subtrack->getRightBottom());
    }
    topBottom.x = minVec.y;
    topBottom.y = maxVec.y;
}

track_gui_entry_t* getParentOf(track_gui_entry_t* t) {

    dbgassert(t);
    dbgassert(t->track);
    dbgassert(t->parent);
    if (t->track->parent) {
        track_gui_entry_t* out = nullptr;
        if (t->parent->getPointerEntry(t->track->parent, &out)) {
            return out;
        }
    }
    return nullptr;
}


void guitrack_topleft::buttonClicked(guibase* _button) {
    if (_button == &btnCopyAutomation) {
        daw_tls::getTls().runtime->copyAutomation = !daw_tls::getTls().runtime->copyAutomation;
    }
    if (_button == &btnFoldAll) {
        isFolded = !isFolded;

        for (track_gui_entry_t* entry : iGuiMgr.getTracksVisibleFlat()) {
            if (entry->parent->parent == nullptr) {
                entry->layout.hideTrack = isFolded;
                updateStoreLoadSubtracks(entry->parent, entry);
            }
        }
        dawCtrl->updateVisibleTrackContents();
    }
}
void guictr_tracks::onChildLayoutChanged(guibase* g) {
    layout();
}
guitrack_topleft::guitrack_topleft(guictr_tracks& _ctrTracks, DawCtrl* const _dawCtrl, track_gui_manager_i& _iGuiMgr, project_t& _project)
    : guictr_base(),
      ctrTracks(_ctrTracks),
      iGuiMgr(_iGuiMgr),
      project(_project) {
    (void) ctrTracks;
    (void) project;
    this->dawCtrl = _dawCtrl;
    padding       = 0;
    btnFoldAll.setLabel("Fold All Tracks");
    btnFoldAll.icon = ICON_ARR_RIGHT;
    btnFoldAll.setStateRef(&isFolded);
    btnFoldAll.getIcon = [gtl = this] { return gtl->isFolded ? ICON_ARR_RIGHT : ICON_ARR_DOWN; };
    btnCopyAutomation.setLabel("Copy+Paste Automation");
    btnCopyAutomation.icon = ICON_AUTOMATION;
    btnCopyAutomation.setStateRef(&daw_tls::getTls().runtime->copyAutomation);
    btnCopyAutomation.colorActive = GuiColor::COL_AUTOMATED;
    guiButtons.push_back(&btnFoldAll);
    guiButtons.push_back(&btnCopyAutomation);
    for (auto guiBtn : guiButtons) {
        add(guiBtn);
    }
}

bool guitrack_mixers::mouseHitTest(ivec2 v, MouseHitEvt& evt) {
    if (this->contains(v)) {
        ivec2 localMouse = this->toContainerSpace(v);
        for (guibase* gui : guis) {
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                if (!evt.getGuiHit())
                    break;
                return true;
            }
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}
