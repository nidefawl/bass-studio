#include <nanovg.h>
#include "assert_dbg.h"
#include "event.h"
#include "fileio.h"
#include "guicolors.h"
#include "host/plugin/base/base-plugin.h"
#include "str_util.h"
#include "tls.h"
#include "trackctr.h"
#include "math/seq_math.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "exceptions.h"
#include "theme.h"
#include "host/automation/automation.h"
#include "host/track/track.h"
#include "trackcontent.h"
#include "trackcontrols.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "basectrl.h"
#include "host/daw/mainctrl.h"
#include "logging.h"
#include "subtrack.h"
#include "appconfig.h"
#include "gui/contextmenu/contextmenu_daw.h"
#include "gui/plugin/pluginctr.h"
#include "gui/views/pluginlist.h"
#include "host/track/trackctr_types.h"

void drawSeperator(NVGcontext* vg, const guitheme_t* theme, int32_t seperatorY, const ivec2& cs) {
    nvgBeginPath(vg);
    nvgMoveTo(vg, 0, seperatorY);
    nvgLineTo(vg, cs.x, seperatorY);
    nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
    nvgStrokeWidth(vg, TRACK_HEIGHT_SPACING);
    nvgStroke(vg);
}

int32_t track_gui_entry_t::getHeight() const {
    if (layout.foldTrack) {
        return math::max<int32_t>(1, CtrSize(track->children));
    }
    return layout.height;
}

void guitrack_controls::render(NVGcontext* vg) {
    if (!setScissorTransform(vg)) {
        return;
    }
    ivec2 cs = getSizeContent();
    nvgBeginPath(vg);
    nvgRect(vg, 0, 0, cs.x, cs.y);
    nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
    nvgFill(vg);

    auto bgImage = theme->getBackgroundImage(GuiBackgroundImage::BG_TRACKEDITOR_MIXERS_1);
    if (bgImage) {
        bgImage->render(this, vg);
    }

    for (track_t* g : project.tracksBottom) {
        track_gui_entry_t* entry = nullptr;
        if (!iGuiMgr.getPointerEntry(g, &entry)) {
            continue;
        }
        if (entry->trackControls->isVisible()) {
            nvgSave(vg);
            entry->trackControls->renderGroupHandle(vg);
            entry->trackControls->render(vg);
            nvgRestore(vg);
        }
    }
    int ySplit = DAW::getPosYFirstReturnTrack(iGuiMgr.getTracksVisibleFlat());
    if (ySplit > 0) {
        nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
        for (track_gui_entry_t* entry : iGuiMgr.getTracksVisibleFlat()) {
            auto& mixer = entry->trackControls;
            if (mixer->isVisible() && mixer->pos.y < ySplit && mixer->bottom() > 0) {
                nvgSave(vg);
                mixer->renderGroupHandle(vg);
                mixer->render(vg);
                nvgRestore(vg);
            }
        }
    }
}

void guitrack_controls::addTrackEntry(track_gui_entry_t& e) {
    this->add(e.trackControls);
}

void guitrack_controls::removeTrackEntry(track_gui_entry_t& e) {
    this->remove(e.trackControls);
    if (dawCtrl) {
        dawCtrl->onTrackMixerRemoved(e);
    }
}

int32_t guictr_tracks::getTrackTotalHeight(track_gui_entry_t* e) {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

    int32_t trH = e->getHeight();
    int32_t totalHeight = trH * TRACK_HEIGHT_STEP;

    if (!e->layout.foldTrack && !e->layout.hideSubtracks) {
        for (auto t2 : e->subtracks) {
            totalHeight += t2->height * TRACK_HEIGHT_STEP + TRACK_HEIGHT_SPACING;
        }
    }
    return totalHeight;
}

int32_t guictr_tracks::setTrackPosition(track_gui_entry_t* e, int32_t y, bool isBottom) {
    if (!assert_expr(e->trackContent)) {
        return 0;
    }
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

    int32_t childTrackInsetX = e->track->getChildLvl() * 8;
    bool bIsFolded   = e->layout.foldTrack;
    ivec2& cntPos    = e->trackContent->pos;
    ivec2& mxrPos    = e->trackControls->pos;
    cntPos           = ivec2(0, y);
    mxrPos           = ivec2(childTrackInsetX, y);
    int32_t trH      = e->getHeight();
    e->trackContent->size = ivec2(trackEditor.size.x, trH * TRACK_HEIGHT_STEP);
    int32_t x2       = e->trackContent->left();
    int32_t y2       = e->trackContent->bottom();
    if (!(bIsFolded || e->layout.hideSubtracks)) {
        for (auto t2 : e->subtracks) {
            int trackheight2 = t2->height * TRACK_HEIGHT_STEP;

            t2->pos  = ivec2(x2, y2);
            t2->size = ivec2(e->trackContent->size.x, trackheight2);

            y2 = t2->bottom() + TRACK_HEIGHT_SPACING;
        }
    } else {
        for (auto t2 : e->subtracks) {
            t2->pos  = ivec2(x2, y2);
            t2->size = ivec2(0, 0);
            y2 = t2->bottom() + TRACK_HEIGHT_SPACING;
        }
    }

    int32_t totalHeight = y2 - y;
    e->trackControls->size = ivec2(trackControls.size.x - childTrackInsetX, totalHeight);

    if (isBottom) {
        cntPos.y -= totalHeight;
        mxrPos.y -= totalHeight;
        for (auto t2 : e->subtracks) {
            t2->pos.y -= totalHeight;
        }
    }
    e->trackContent->positionChanged();
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
    trackEditor.addSubtrack(entry, subtrack, insertFront);
    if (entry->trackControls) entry->trackControls->addSubtrackMixer(entry, subtrack);
}

void guictr_tracks::removeSubtrack(track_gui_entry_t* entry, gui_track_subtrack* subtrack) {
    trackEditor.removeSubtrack(entry, subtrack);
    if (entry->trackControls) entry->trackControls->removeSubtrackMixer(subtrack);
}

gui_track_automationlane* guictr_tracks::addAutomationLane(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx, bool insertFront) {
    auto* al = new gui_track_automationlane(entry, m_grid, at, paramIdx);
    addSubTrack(entry, al, insertFront);
    return al;
}

void guictr_tracks::removeAutomationLane(gui_track_automationlane* al) {
    track_gui_entry_t* entry = nullptr;
    always_assert(guiMgr.getTrackEntry(al->m_track, &entry));
    if (entry->trackControls) entry->trackControls->removeSubtrackMixer(al);
    trackEditor.removeSubtrack(entry, al);
}

void guictr_tracks::removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx) {
    if (entry->trackControls) entry->trackControls->removeAllAutomationLanes(at, paramIdx);
    trackEditor.removeAllAutomationLanes(entry, at, paramIdx);
}

void guictr_tracks::removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at) {
    if (entry->trackControls) entry->trackControls->removeAllAutomationLanes(at);
    trackEditor.removeAllAutomationLanes(entry, at);
}

void guictr_tracks::removeAllSubtracks(track_gui_entry_t* entry) {
    if (entry->trackControls) entry->trackControls->removeAllSubtracks();
    trackEditor.removeAllSubtracks(entry);
}

void guictr_tracks::resetView() {
    trackEditor.m_resizePreModifyState.reset();
    trackEditor.action.clipboard.reset();
    trackEditor.iGuiMgr.reset();
}

void loadSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, const track_layout_snapshot_t& snapshot);
void loadTrackLayoutSettings(track_gui_entry_t* entry, const tracklayout_settings_t& settings);

void loadTrackLayout(guictr_tracks* guiTracks, track_gui_entry_t* entry, const track_layout_snapshot_t& snapshot) {
    entry->subtracks.clear();
    bool hide = entry->layout.hideSubtracks || entry->layout.foldTrack;
    if (hide) {
        entry->state.layoutSaved = snapshot;
        entry->state.wasInHide   = true;
    } else {
        entry->state.wasInHide = false;
        loadTrackLayoutSettings(entry, snapshot.layout);
        loadSubtrackLayout(guiTracks, entry, snapshot);
        entry->state.layoutSaved = track_layout_snapshot_t();
    }
}

void guictr_tracks::loadTrackLayouts(trackcontainer_snapshot_t& in) {
    for (track_snapshot_t& snapshot : in.tracks) {
        dbgassert(snapshot.trackLoaded);
        auto it = snapshot.layouts.find(trackContainerGlobalIndex);
        if (it != snapshot.layouts.end()) {
            track_layout_snapshot_t& layout = it->second;
            track_gui_entry_t* entry{};
            if (guiMgr.getTrackEntry(snapshot.trackLoaded, &entry))
                loadTrackLayout(this, entry, layout);
        }
        // trackStatic.trackLoaded = nullptr;
    }
}

void guictr_tracks::scrollOffsetChanged(int dir, float offset) {
    int32_t scrOffset = math::max(0.0f, offset * (contentHeight - contentViewSize));

    int y = TRACK_HEIGHT_SPACING - scrOffset;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            int32_t h = setTrackPosition(entry, y, false);
            y += h + TRACK_HEIGHT_SPACING;
        }
    }
}

void guictr_tracks::scrollTo(guibase* g) {
    int32_t scrOffset = math::max(0.0f, scrollbar.scrollOffset * (contentHeight - contentViewSize));

    int32_t y = g->pos.y;
    scrollbar.scrollVisible(y + scrOffset, g->size.y);
}

void guictr_tracks::updateVisibleTracks() {
    for (auto entry : guiMgr.entries) {
        auto tr = entry->track;
        bool bIsVisible = bShowMasterTracks || tr->type != TRACK_TYPE_MASTER;
        bIsVisible = bIsVisible && (bShowReturnTracks || tr->type != TRACK_TYPE_RETURN);
        if (entry->isHidden == bIsVisible) {
            entry->isHidden = !bIsVisible;
            entry->trackControls->setVisible(bIsVisible);
            entry->trackContent->setVisible(bIsVisible);
        }
    }
    guiMgr.updateVisibleTracks(project.trackList);
    track_gui_vector_td& tracks = guiMgr.tracksVisibleFlat;
    for (track_gui_entry_t* entry : tracks) {
        entry->trackContent->updateVisibleTrackContents(m_grid);
        for (gui_track_subtrack* au : entry->subtracks) {
            au->updateVisibleTrackContents(m_grid);
        }
    }
}

void guictr_tracks::layoutVisibleTracks() {
    track_gui_vector_td& tracks = guiMgr.tracksVisibleFlat;
    for (track_gui_entry_t* entry : tracks) {
        entry->trackContent->updateVisibleTrackContents(m_grid);
        for (gui_track_subtrack* au : entry->subtracks) {
            au->updateVisibleTrackContents(m_grid);
        }
    }
}

void guictr_tracks::layout() {
    const int32_t trackControlsWidth = theme->get(GuiConstant::CONST_TRACK_CONTROLS_WIDTH);

    bool trackCtrlsLeft = true;
    int scrollW         = gui_scrollbar::defaultW;

    ivec2 cs       = getSizeContent();
    cs.x = math::max(scrollW+trackControlsWidth+5, cs.x);
    cs.y = math::max(64, cs.y);
    scrollbar.pos  = ivec2(cs.x - scrollW, 0);
    scrollbar.size = ivec2(scrollW, cs.y);

    trackTimeline.pos  = ivec2(trackCtrlsLeft ? trackControlsWidth : 0, 0);
    trackTimeline.pos  = ivec2(trackCtrlsLeft ? trackControlsWidth : 0, 0);
    trackTimeline.size = ivec2(cs.x - trackControlsWidth, 32);
    loophandles.pos    = ivec2(trackTimeline.left(), trackTimeline.bottom());
    loophandles.size   = ivec2(trackTimeline.size.x, heightTimelineControls);
    trackTopLeft.pos   = ivec2(trackCtrlsLeft ? 0 : cs.x - trackControlsWidth, 0);
    trackEditor.pos      = ivec2(trackCtrlsLeft ? trackControlsWidth : 0, loophandles.bottom());
    trackControls.pos  = ivec2(trackCtrlsLeft ? 0 : cs.x - trackControlsWidth, loophandles.bottom());
    trackEditor.size     = ivec2(cs.x - trackControlsWidth, cs.y - loophandles.bottom());
    trackTopLeft.size  = ivec2(trackControlsWidth, loophandles.bottom());
    trackControls.size = ivec2(trackControlsWidth, trackEditor.size.y);
    loophandles.clipViewSize = ivec2(trackEditor.size.x, trackEditor.size.y + loophandles.size.y);
    m_grid.update(trackEditor.size);

    ivec2 csTrackView = trackEditor.getSizeContent();

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
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    contentHeight += TRACK_HEIGHT_STEP * 4;

    int32_t scrOffset = math::max(0.0f, getScrollOffset() * (contentHeight - contentViewSize));

    int y = TRACK_HEIGHT_SPACING - scrOffset;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            int32_t h = setTrackPosition(entry, y, false);
            y += h + TRACK_HEIGHT_SPACING;
        }
    }


    y = csTrackView.y - TRACK_HEIGHT_SPACING;

    itMastersTracks = guiMgr.trackEntriesBottom.rbegin();
    itMastersEnd    = guiMgr.trackEntriesBottom.rend();
    while (itMastersTracks != itMastersEnd) {
        auto& entry = *itMastersTracks;
        if (guiMgr.isVisible(entry)) {
            dbgassert(entry->trackContent);
            int32_t h = setTrackPosition(entry, y, true);
            y -= h;
            y -= TRACK_HEIGHT_SPACING;
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
    trackEditor.render(vg);
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

    ivec2 trackViewInnerSize = trackEditor.getSizeContent();
    if (trackViewInnerSize.y > 1 && trackViewInnerSize.x > 1) {
        nvgSave(vg);
        nvgTranslate(vg, 0, trackEditor.top());
        int ySplit = DAW::getPosYFirstReturnTrack(guiMgr.tracksVisibleFlat);
        track_gui_entry_t* lastEntry = nullptr;
        if (ySplit > 0) {
            nvgSave(vg);
            nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
            for (track_gui_entry_t* entry : guiMgr.getTracksTopFlat()) {
                drawSeperator(vg, theme, entry->trackControls->bottom() + TRACK_HEIGHT_SPACING_HALF, cs);
                lastEntry = entry;
            }
            nvgRestore(vg);
        }
        auto& tracksBottm = guiMgr.getTracksBottomFlat();
        if (!tracksBottm.empty() && (ySplit <= 0 || trackEditor.size.y > ySplit)) {
            if (ySplit > 0) {
                nvgIntersectScissor(vg, 0, ySplit, cs.x, trackEditor.size.y - ySplit);
            } else {
                nvgIntersectScissor(vg, 0, 0, cs.x, trackEditor.size.y);
            }
            for (track_gui_entry_t* entry : tracksBottm) {
                drawSeperator(vg, theme, entry->trackControls->top() - TRACK_HEIGHT_SPACING_HALF, cs);
            }
        }
        nvgRestore(vg);

        dragdrop_target_indicator_t& dragDropTarget = dawCtrl->getDragDropTarget();
        const auto dragdropTargetGui = safeRefGet(dragDropTarget.target);
        if (dragdropTargetGui && (dragdropTargetGui == this || dragdropTargetGui->parent == &trackControls || dragdropTargetGui->parent == &trackEditor)) {
            nvgSave(vg);
            nvgTranslate(vg, 0, trackEditor.top());
            int n       = this->theme->get(GuiConstant::CONST_GUI_INSET_WIDGET_BG);
            auto bgPos  = ivec2(n);
            auto bgSize = this->getSizeContent() - ivec2(n * 2);
            if (bgSize.x > 0 && bgSize.y > 0) {
                nvgGlobalAlpha(vg, 0.5f);
                nvgBeginPath(vg);
                nvgRect(vg, bgPos.x, bgPos.y, bgSize.x, bgSize.y);
                if (dragdrop_target_indicator_t::target_area == dragDropTarget.type) {
                    nvgPathWinding(vg, NVGwinding::NVG_CW);
                    if (dragDropTarget.slotIdx == -2) {
                        // render at end of tracks 
                        if (ySplit > 0 && lastEntry) {
                            auto trackDefaultHeight = 4;
                            const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
                            const auto trackHeight = trackDefaultHeight * TRACK_HEIGHT_STEP;
                            nvgRect(vg, bgPos.x, lastEntry->trackControls->bottom() + TRACK_HEIGHT_SPACING_HALF*2.0f, bgSize.x, trackHeight);
                            dragDropTarget.targetPos = ivec2(0, lastEntry->trackControls->bottom() + TRACK_HEIGHT_SPACING_HALF*2.0f + trackHeight * 0.5f);
                        }
                    } else {
                        nvgRect(vg, dragdropTargetGui->pos.x, dragdropTargetGui->pos.y, dragdropTargetGui->size.x, dragdropTargetGui->size.y);
                    }
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
            auto desc = dragDropTarget.desc;
            if (desc.empty()) {
                // desc = "Drop " dragdropTargetGui->getLabel() + StringFormat("[%d]", dragDropTarget.slotIdx)
                desc = "Drop here";
            }
            renderCenteredMultilineText(vg, theme, desc, fontScale, getLabelColor(), indicatorPos, ivec2(titleHeight * 30, titleHeight * 2));

            nvgRestore(vg);
        }
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
        trackEditor.renderDebugPass(vg);
    }

    if (trackViewInnerSize.y > 1 && trackViewInnerSize.x > 1) {
        nvgIntersectScissor(vg, trackEditor.pos.x, 0, trackEditor.size.x, cs.y);
        nvgTranslate(vg, trackEditor.pos.x, 0);
        tick_t pos = dawCtrl->getDaw()->getPlaybackPos();

        float playBackX = (float) m_grid.tickToScreenD(pos);
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

guitrack_editor::guitrack_editor(DawCtrl* const _dawCtrl, track_gui_manager_i& _iGuiMgr, DAW::Cursor& _cursor, project_t& _project, project_globals_t& _projectGlobals, scaled_grid& _grid, dragdrop_file& _dragdropclip)
    : guictr_base(),
      iGuiMgr(_iGuiMgr),
      cursor(_cursor),
      project(_project),
      projectGlobals(_projectGlobals),
      grid(_grid),
      dragdrop(_dragdropclip) {
    setGuiType(gui_type::CTR_TYPE_TRACKS_EDITOR);
    this->dawCtrl = _dawCtrl;
    padding       = 0;
    sortChildren  = true;
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

bool guitrack_editor::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 localMouse = this->toContainerSpace(mpos);
        for (guibase* gui : guis) {
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                if (!evt.getGuiHit()) 
                    break;
                return true;
            }
        }
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) return false;
        if (evt.type == MOUSE_DRAGDROP_FILE) {
            auto clipboard = dawCtrl->getDaw()->getDragDropClip();
            switch (clipboard.type) {
                case dragdrop_file::TYPE_AUDIOFILE:
                case dragdrop_file::TYPE_CLIP:
                    evt.requestFocus(this);
                    return true;
                default:
                    break;
            }
            return false;
        }
        evt.requestFocus(this);
        return true;
    }
    return false;
}

void guitrack_editor::addTrackEntry(track_gui_entry_t& e) {
    add(e.trackContent);
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
    if (entry.trackContent) {
        if (dawCtrl) {
            dawCtrl->onTrackContentRemoved(entry);
        }
        entry.trackContent->destroyGuis();
        remove(entry.trackContent);
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

void guictr_tracks::removeTrack(track_t* track, int flags) {
    track_gui_entry_t* entry = nullptr;
    if (!guiMgr.getPointerEntry(track, &entry)) {
        return;
    }
    dbgassert(track->audio);
    removeAllSubtracks(entry);
    trackControls.removeTrackEntry(*entry);
    trackEditor.removeTrackEntry(*entry);
    removeEntry(track->audio->guiInstances, entry);
    dbgassert(entry->trackContent);
    dbgassert(entry->trackControls);
    delete entry->trackContent;
    delete entry->trackControls;
    entry->trackContent = nullptr;
    entry->trackControls   = nullptr;
    guiMgr.removeTrack(*entry); // does delete entry
}

void guictr_tracks::addTrack(track_t* track, int flags) {
    dbgassert(track->audio);
    auto* entry = new track_gui_entry_t{};

    entry->parentCtrl = this->dawCtrl;
    entry->track      = track;
    entry->parent     = this;
    entry->trackControls = DAW::createTrackGuiControls(entry, m_grid);
    entry->trackContent  = DAW::createTrackGui(entry, m_grid);

    guiMgr.addTrack(entry);
    trackControls.addTrackEntry(*entry);
    trackEditor.addTrackEntry(*entry);
    track->audio->guiInstances.push_back(entry);

    //TODO: restore subtracks
    if (!(flags & FLG_TRK_CHANGE_LOAD)) {
        updateVisibleTracks();
        layout();
    }
}

void guitrack_controls::handleRightClick(MouseEvent& evt) {
    if (!assert_expr(dawCtrl)) {
        return;
    }
    parentCtrl->openContextMenu(new guictxtmenu_notrack(dawCtrl), evt.mousepos);
}

void getTrackGuiYBounds(const track_gui_entry_t* track, ivec2& topBottom) {
    auto minVec = math::minvec2(track->trackControls->getLeftTop(), track->trackContent->getLeftTop());
    auto maxVec = math::maxvec2(track->trackControls->getRightBottom(), track->trackContent->getRightBottom());
    for (auto subtrack : track->subtracks) {
        maxVec = math::maxvec2(maxVec, subtrack->getRightBottom());
    }
    topBottom.x = minVec.y;
    topBottom.y = maxVec.y;
}

void guitrack_topleft::buttonClicked(guibase* _button) {
    if (_button == &btnCopyAutomation) {
        daw_tls::getTls().runtime->copyAutomation = !daw_tls::getTls().runtime->copyAutomation;
    }
    if (_button == &btnFoldAll) {
        isFolded = !isFolded;

        for (track_gui_entry_t* entry : iGuiMgr.getTracksVisibleFlat()) {
            if (!isFolded && TRACKTYPE_TO_CTR(entry->track->type) != TRACK_CTR_MIDIAUDIO) {
                continue;
            }
            entry->layout.foldTrack = isFolded;
            updateStoreLoadSubtracks(entry->parent, entry);
        }
        dawCtrl->updateVisibleTrackContents();
    }
    if (_button == &btnShowReturnTracks) {
        ctrTracks.bShowReturnTracks = !ctrTracks.bShowReturnTracks;
        dawCtrl->updateVisibleTrackContents();
    }
    if (_button == &btnShowMasterTracks) {
        ctrTracks.bShowMasterTracks = !ctrTracks.bShowMasterTracks;
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
    btnShowReturnTracks.setLabel("Show Return Tracks");
    btnShowReturnTracks.icon = ICON_MODULATION_INPUT;
    btnShowReturnTracks.setStateRef(&ctrTracks.bShowReturnTracks);
    btnShowReturnTracks.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
    btnShowMasterTracks.setLabel("Show Master Tracks");
    btnShowMasterTracks.icon = ICON_ADJUST;
    btnShowMasterTracks.setStateRef(&ctrTracks.bShowMasterTracks);
    btnShowMasterTracks.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
    guiButtons.push_back(&btnFoldAll);
    guiButtons.push_back(&btnCopyAutomation);
    guiButtons.push_back(&btnShowReturnTracks);
    guiButtons.push_back(&btnShowMasterTracks);
    for (auto guiBtn : guiButtons) {
        add(guiBtn);
    }
}

bool guitrack_controls::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    if (this->contains(mpos)) {
        ivec2 localMouse = this->toContainerSpace(mpos);
        // iterate over guis vector in reverse
        for (auto it = guis.begin(); it != guis.end(); ++it) {
            auto gui = *it;
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (evt.type == MOUSE_DRAGDROP_FILE) {
            return false;
        }
        if (evt.type == MOUSE_DRAGDROP_OBJECT) {
            return false;
        }
        if (evt.type == MouseHitType::MOUSE_SCROLL) {
            evt.requestFocus(this);
            return true;
        }
        if (canMouseHit()) {
            evt.requestFocus(this);
            return true;
        }
    }
    return false;
}

bool guictr_tracks::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    if (trackEditor.handleEditorCommand(ctxt)) {
        return true;
    }
    return trackControls.handleEditorCommand(ctxt);
}

bool guitrack_controls::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    if (ctxt.type == GlobalCommandType::CMD_BEGIN_RENAME) {
        if (ctxt.kevt.type != KeyboardState::K_PRESS) {
            return true;
        }
        auto selTrack = dawCtrl->getSelectedTrack();
        track_gui_entry_t* entry = nullptr;
        if (iGuiMgr.getPointerEntry(selTrack, &entry)) {
            DAW::OpenRenameTrackPopup(dawCtrl, entry);
        }
        return true;
    }
    return false;
}

bool guitrack_controls::handleKeyInput(KeyEvent& kevt) {
    if (kevt.cmd) {
        auto temp = kevt.cmd->getKeybindContextData(kevt);
        if (handleEditorCommand(temp)) {
            return true;
        }
    }
    if (isArrowKey(kevt.keyCode)) {
        ivec2 dir;
        arrowKeyToXY(kevt.keyCode, dir.x, dir.y);
        DAW::UI::CommandContext ctxt = {GlobalCommandType::CMD_MOVE_CURSOR, kevt, dir.x, dir.y};
        if (handleEditorCommand(ctxt)) {
            return true;
        }
    }
    return false;
}

void guictr_tracks::onAdded() {
    guictr_base::onAdded();
}

void guictr_tracks::onRemove() {
    guictr_base::onRemove();
}

void guictr_tracks::addAllTracks() {
    for (track_t* tr : project.trackList) {
        if (!guiMgr.getTrackEntry(tr, nullptr)) {
            addTrack(tr, FLG_TRK_CHANGE_LOAD);
        }
    }
}

void guictr_tracks::removeAllTracks() {
    track_gui_vector_td tracksCopy = guiMgr.getTracksVisibleFlat();
    for (auto* entry : tracksCopy) {
        removeTrack(entry->track, FLG_TRK_CHANGE_LOAD);
    }
}

guictr_tracks::guictr_tracks(DawCtrl* _dawCtrl, DAW::Cursor& _cursor, DAW::TrackSelection& _trackSelection, project_t& _project, project_globals_t& _projectGlobals, dragdrop_file& _dragdropclip)
    : guictr_base(),
      trackContainerGlobalIndex(_dawCtrl->getDawWindowIndex()),
      project(_project),
      projectGlobals(_projectGlobals),
      guiMgr(),
      trackTopLeft(*this, _dawCtrl, guiMgr, _project),
      trackControls(guiMgr, _project),
      trackEditor(_dawCtrl, guiMgr, _cursor, _project, _projectGlobals, m_grid, _dragdropclip),
      trackTimeline(m_grid),
      loophandles(_project, _projectGlobals, m_grid),
      scrollbar(1, 0.0f, *this) {
    padding = 2;
    margin  = 2;
    setGuiType(gui_type::CTR_TYPE_TRACKS);
    dawCtrl = _dawCtrl,
    setCanMouseHit(true);
    setBackgroundRendered(true);
    m_grid.addCallback(this);
    add(&trackTimeline);
    add(&loophandles);
    add(&trackTopLeft);
    add(&trackControls);
    add(&trackEditor);
    add(&scrollbar);
}

guictr_tracks::~guictr_tracks() {
    removeAllTracks();
    remove(&scrollbar);
    remove(&trackEditor);
    remove(&trackControls);
    remove(&trackTopLeft);
    remove(&loophandles);
    remove(&trackTimeline);
}

bool guictr_tracks::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    if (isCtrl(evt.kbmods)) {
        float zomDelta   = 1.0f + yoffset * -0.2f;
        ivec2 localMouse = trackTimeline.toContainerSpace(evt.relMousepos);
        trackTimeline.adjustZoom(localMouse.x, zomDelta);
        return true;
    } else if (isShift(evt.kbmods)) {
        trackTimeline.adjustOffset(-yoffset * 32);
        return true;
    }
    return scrollbar.handleMouseScroll(evt, xoffset, yoffset);
}

bool guictr_tracks::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
    bool bContains = this->contains(mpos);
    if (bContains) {
        ivec2 localMouse = this->toContainerSpace(mpos);
        // iterate over guis vector in reverse
        for (auto it = guis.rbegin(); it != guis.rend(); ++it) {
            auto gui = *it;
            if (gui->isVisible() && gui->mouseHitTest(localMouse, evt)) {
                return true;
            }
        }
        if (evt.type == MouseHitType::MOUSE_SCROLL) {
            evt.requestFocus(this);
            return true;
        }
        if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT && evt.getDraggedThing()) {
            auto type = evt.getDraggedThing()->getGuiType();
            switch (type) {
                case gui_type::CTR_TYPE_PLUGINS_DRAGGED:
                case gui_type::CTR_TYPE_PLUGINS_LIST_ENTRY:
                case gui_type::CTR_TYPE_TRACK_TITLE:
                    evt.requestFocus(this);
                    return true;
                default:
                    break;
            }
            return false;
        }
        if (evt.type == MOUSE_DRAGDROP_FILE) {
            auto clipboard = dawCtrl->getDaw()->getDragDropClip();
            switch (clipboard.type) {
                case dragdrop_file::TYPE_PLUGIN_PRESET:
                case dragdrop_file::TYPE_TRACK_CONTAINER:
                    evt.requestFocus(this);
                    return true;
                default:
                    break;
            }
        }
        if (canMouseHit()) {
            evt.requestFocus(this);
            return true;
        }
    }
    return false;
}

void guictr_tracks::pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) {
    String clipboardDescription = StringFormat("%d Plugins", CtrSize(g->effects));
    DAW::gui_track_drop_position_t slot = DAW::GetTrackSlotFromCoord(this, trackControls.toContainerSpace(mousepos), false);
    if (slot.droptype == DAW::gui_track_drop_position_t::none) {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{
            dragdrop_target_indicator_t::target_area,
            -2,
            toRef(),
            this->pos + this->size/2,
            "Move " + clipboardDescription + " to new track"
        };
        return;
    }

    dawCtrl->getDragDropTarget().reset();

    if (slot.droptype != DAW::gui_track_drop_position_t::track_on) {
        return;
    }

    auto dstTrack = slot.droppedTrack;
    if (!dstTrack)
        return;

    audio_stage_t* srcStage = g->getTrackLink();
    audio_stage_t* dstStage = dstTrack->getStage();
    int highlightSlot = CtrSize(dstStage->effects);

    if (dstStage == srcStage) {
        int first = g->effects.front()->getSlot();
        int last  = g->effects.back()->getSlot();
        if (highlightSlot >= first && highlightSlot <= last) {
            return;
        }
    } else {
        // prevent dragging onto if any of the effects is parent of this
        audio_stage_t* p = dstStage;
        while (p) {
            if (p->owner && std::find(g->effects.begin(), g->effects.end(), p->owner) != g->effects.end()) {
                // NOTE: I think this can never happen here, because we're dragging from a track to another track
                return;
            }
            p = p->parent;
        }
    }

    track_gui_entry_t* dstTrackEntry = nullptr;
    if (!this->guiMgr.getTrackEntry(dstTrack, &dstTrackEntry))
        return;

    dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{
        dragdrop_target_indicator_t::target_area,
        highlightSlot,
        dstTrackEntry->trackControls->toRef(),
        slot.pos, 
        "Move " + clipboardDescription + " to " + dstTrack->name
    };
}

void guictr_tracks::pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) {
    String clipboardDescription = g->getLabel();
    DAW::gui_track_drop_position_t slot = DAW::GetTrackSlotFromCoord(this, trackControls.toContainerSpace(mousepos), false);
    if (slot.droptype == DAW::gui_track_drop_position_t::none) {
        dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{
            dragdrop_target_indicator_t::target_area,
            -2,
            toRef(),
            this->pos + this->size/2,
            "Insert " + clipboardDescription + " on new track"
        };
        return;
    }

    dawCtrl->getDragDropTarget().reset();

    if (slot.droptype != DAW::gui_track_drop_position_t::track_on) {
        return;
    }

    auto dstTrack = slot.droppedTrack;
    if (!dstTrack)
        return;

    audio_stage_t* dstStage = dstTrack->getStage();
    int highlightSlot = CtrSize(dstStage->effects);

    track_gui_entry_t* dstTrackEntry = nullptr;
    if (!this->guiMgr.getTrackEntry(dstTrack, &dstTrackEntry))
        return;

    dawCtrl->getDragDropTarget() = dragdrop_target_indicator_t{
        dragdrop_target_indicator_t::target_area,
        highlightSlot,
        dstTrackEntry->trackControls->toRef(),
        slot.pos, 
        "Insert " + clipboardDescription + " on " + dstTrack->name
    };
}

namespace DAW {
    void InsertDraggedPluginsOnTrack(DawInstance* daw, track_t* track, guictr_dragged_plugins* g) {
        dbgassert(g->effects.size());

        audio_stage_t* srcStage = g->getTrackLink();
        audio_stage_t* dstStage = track->getStage();

        // make sure this pluginctrs stage-owner and all parents stage-owners aren't in the list of dragged effectbase instances
        // this is to avoid i.e. dragging a group into itself
        // this might not be necessary anymore, but it's a good check to have
        auto dstStageOrParent = dstStage;
        while (dstStageOrParent) {
            if (!dstStageOrParent->parent) {
                dbgassert(dstStageOrParent->owner == nullptr);
            }
            if (dstStageOrParent->parent && std::find(g->effects.begin(), g->effects.end(), dstStageOrParent->owner) != g->effects.end()) {
                return;
            }
            dstStageOrParent = dstStageOrParent->parent;
        }

        auto targetslot = g->effects.front()->isSynth ? 0 : CtrSize(dstStage->effects);


        ThreadLock lock = daw->lockPlayThread();
        int first       = g->effects.front()->getSlot();
        int last        = g->effects.back()->getSlot();
        if (srcStage == dstStage) {
            if (targetslot >= first && targetslot <= last) {
                return;
            }
        }
        if (targetslot >= 0) {
            if (srcStage != dstStage) {
                daw->getPluginManager()->movePluginsToStage(dstStage, srcStage, first, targetslot, last - first + 1);
                audio_stage_ref_t refsrc = srcStage->toRef();
                audio_stage_ref_t refdst = dstStage->toRef();
                daw->pushHist(new action_move_modules("Move plugin", refdst, refsrc, targetslot, first, last - first + 1));
            } else {
                if (targetslot > first) targetslot -= CtrSize(g->effects);
                if (first == targetslot)
                    return;
                daw->getPluginManager()->movePluginsOnStage(dstStage, first, targetslot, last - first + 1);
                audio_stage_ref_t ref = dstStage->toRef();
                daw->pushHist(new action_shift_modules("Move plugin", ref, targetslot, first, last - first + 1));
            }
            daw->onPluginsChanged();
            for (auto& gui : dstStage->gui) {
                gui->scrollToPluginGui(g->effects.back());
            }
        }
    }

    void InsertPluginOnTrack(DawInstance* daw, track_t* track, effectbase* effect) {
        auto pluginMgr = daw->getPluginManager();
        if (assert_expr(effect)) {
            ThreadLock lock = daw->lockPlayThread();
            int32_t dstSlot = effect->isSynth ? 0 : CtrSize(track->getStage()->effects);
            pluginMgr->insertNewPlugin(track->getStage(), effect, dstSlot);
            effect->onEnable();
            daw->pushHist(new action_insert_effect("Insert plugin", effect, track->getStage()->toRef(), dstSlot));
            daw->onPluginsChanged();
            for (auto& gui : track->getStage()->gui) {
                gui->scrollToPluginGui(effect);
            }
        }
    }
} // namespace DAW

void guictr_tracks::pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) {
    auto daw = dawCtrl->getDaw();
    String clipboardDescription = g->getLabel();
    DAW::gui_track_drop_position_t slot = DAW::GetTrackSlotFromCoord(this, trackControls.toContainerSpace(mousepos), false);

    if (slot.droptype == DAW::gui_track_drop_position_t::none) {
        ThreadLock lock = daw->lockPlayThread();
        auto track = daw->insertNewTrack(-1, TRACK_TYPE_MIDI);
        if (!track)
            return;
        track_gui_entry_t* entry = nullptr;
        if (!this->guiMgr.getTrackEntry(track, &entry))
            return;
        dawCtrl->setSelectedTrackEntry(entry);
        dawCtrl->showPluginView();
        dawCtrl->getDragDropTarget().reset();
        auto effect = g->makeInstance();
        if (effect) {
            DAW::InsertPluginOnTrack(daw, track, effect);
            track->name = effect->getName();
        }
        return;
    }

    if (slot.droptype != DAW::gui_track_drop_position_t::track_on) {
        return;
    }

    auto dstTrack = slot.droppedTrack;
    if (!dstTrack)
        return;

    auto effect = g->makeInstance();
    if (effect) {
        ThreadLock lock = daw->lockPlayThread();
        DAW::InsertPluginOnTrack(daw, dstTrack, effect);
    }
}

void guictr_tracks::pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) {
    auto daw = dawCtrl->getDaw();
    String clipboardDescription = StringFormat("%d Plugins", CtrSize(g->effects));
    DAW::gui_track_drop_position_t slot = DAW::GetTrackSlotFromCoord(this, trackControls.toContainerSpace(mousepos), false);
    if (slot.droptype == DAW::gui_track_drop_position_t::none) {
        ThreadLock lock = daw->lockPlayThread();
        auto track = daw->insertNewTrack(-1, TRACK_TYPE_MIDI);
        if (!track)
            return;
        track_gui_entry_t* entry = nullptr;
        if (!this->guiMgr.getTrackEntry(track, &entry))
            return;
        dawCtrl->setSelectedTrackEntry(entry);
        dawCtrl->showPluginView();
        dawCtrl->getDragDropTarget().reset();
        DAW::InsertDraggedPluginsOnTrack(daw, track, g);
        return;
    }
    if (slot.droptype != DAW::gui_track_drop_position_t::track_on) {
        return;
    }
    auto dstTrack = slot.droppedTrack;
    if (!dstTrack)
        return;
    audio_stage_t* srcStage = g->getTrackLink();
    audio_stage_t* dstStage = dstTrack->getStage();
    if (dstStage == srcStage) {
        return;
    }
    ThreadLock lock = daw->lockPlayThread();
    DAW::InsertDraggedPluginsOnTrack(daw, dstTrack, g);
}

bool guictr_tracks::fileDropMove(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) {
    auto clipboard = dawCtrl->getDaw()->getDragDropClip();
    switch (clipboard.type) {
        case dragdrop_file::TYPE_PLUGIN_PRESET:
        case dragdrop_file::TYPE_TRACK_CONTAINER:
            DAW::SetDragDropTrackInidicatorFromMousePos(this, trackControls.toContainerSpace(mousepos), clip.path, clipboard.type == dragdrop_file::TYPE_TRACK_CONTAINER);
            clip.isValidTarget = true;
            clip.target = makeSafeRef();
            return true;
        default:
            break;
    }
    return false;
}

bool guictr_tracks::fileDropRelease(dragdrop_file& clip, ivec2 mousepos, KeyboardMods kbmods) {
    auto daw = dawCtrl->getDaw();
    auto clipboard = daw->getDragDropClip();
    DAW::gui_track_drop_position_t slot = DAW::GetTrackSlotFromCoord(this, trackControls.toContainerSpace(mousepos), clipboard.type == dragdrop_file::TYPE_TRACK_CONTAINER);
    switch (clipboard.type) {
        case dragdrop_file::TYPE_PLUGIN_PRESET: {
            ThreadLock lock = daw->getPlayThread()->lockThread();
            track_t* track = nullptr;
            bool bSetName = false;
            if (slot.droptype == DAW::gui_track_drop_position_t::drop_type::none) {
                track = daw->insertNewTrack(-1, TRACK_TYPE_MIDI);
                bSetName = true;
            } else if (slot.droppedTrack) {
                track = slot.droppedTrack;
            }
            if (!track)
                return false;
            track_gui_entry_t* entry = nullptr;
            if (!this->guiMgr.getTrackEntry(track, &entry))
                return false;
            dawCtrl->setSelectedTrackEntry(entry);

            audio_stage_t* dstStage = track->getStage();
            auto* pluginMgr = daw->getPluginManager();
            auto pluginSnapshot = clip.pluginSnapshot.get();
            DAW::assignFreeStageIds(pluginMgr, *pluginSnapshot);
            auto effect = pluginMgr->loadPluginDeferred(*pluginSnapshot);
            if (effect) {
                ThreadLock lock = daw->getPlayThread()->lockThread();
                if (bSetName) {
                    track->name = effect->getDfrdPluginName();
                }
                DAW::InsertEffectDeferredOnStage(daw, dstStage, effect, -2, true, true);
            }
            return true;
        }
        case dragdrop_file::TYPE_TRACK_CONTAINER: {
            ThreadLock lock = daw->getPlayThread()->lockThread();
            DAW::InsertTrackContainerOnTrack(daw, clip.trackcontainer.get(), slot);
            return true;
        }
        default:
            break;
    }
    return false;
}

void guictr_tracks::trackEntryDragMove(track_gui_entry_t* trackEntry, ivec2 mousepos) {
    DAW::SetDragDropTrackInidicatorFromMousePos(this, trackControls.toContainerSpace(mousepos), trackEntry->track->name, true);
}

void guictr_tracks::trackEntryDragRelease(track_gui_entry_t* trackEntry, ivec2 mousepos) {
    DAW::gui_track_drop_position_t slot = DAW::GetTrackSlotFromCoord(this, trackControls.toContainerSpace(mousepos), true);
    DAW::MoveTrackToSlot(parent->dawCtrl->getDaw(), trackEntry->track, slot);
}
