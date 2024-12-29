#include <nanovg.h>
#include "assert_dbg.h"
#include "gui/container/container.h"
#include "gui/gui.h"
#include "trackctr.h"
#include "trackcontent.h"


guictr_mixers::guictr_mixers(DawCtrl* _dawCtrl, DAW::Cursor& _cursor, DAW::TrackSelection& _trackSelection, project_t& _project, project_globals_t& _projectGlobals, dragdrop_file& _dragdropclip)
    : guictr_base(),
      trackMixerGlobalIndex(_dawCtrl->getDawWindowIndex()),
      project(_project),
      projectGlobals(_projectGlobals),
      guiMgr(),
      trackMixers(),
      scrollbar(0, 0.0f, *this) {
    padding = 2;
    margin  = 2;
    setGuiType(gui_type::CTR_TYPE_MIXERS);
    dawCtrl = _dawCtrl,
    setCanMouseHit(true);
    setBackgroundRendered(true);
    padding = 0;
    margin  = 0;
    setCanMouseHit(false);
    setBackgroundRendered(false);
    add(&trackMixers);
    add(&scrollbar);
}

guictr_mixers::~guictr_mixers() {
    remove(&scrollbar);
    remove(&trackMixers);
}

int32_t guictr_mixers::getTrackTotalWidth(track_gui_entry_t* e) {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    int32_t trH = e->getHeight();
    int32_t totalHeight = trH * TRACK_HEIGHT_STEP;
    return totalHeight;
}

int32_t guictr_mixers::setTrackPosition(track_gui_entry_t* e, int32_t x, bool isBottom) {
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    int32_t trH      = e->getHeight();
    e->trackMixers->size = ivec2(trH * TRACK_HEIGHT_STEP, trackMixers.size.y);
    if (isBottom) {
        e->trackMixers->pos  = ivec2(x - e->trackMixers->size.x , 0);
    } else {
        e->trackMixers->pos  = ivec2(x, 0);
    }
    return e->trackMixers->size.x;
}

bool guictr_mixers::mouseHitTest(ivec2 mpos, MouseHitEvt& evt) {
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


void guictr_mixers::render(NVGcontext* vg) {
    if (isBackgroundRendered()) {
        renderBackground(vg);
    }
    ivec2 cs = getSizeContent();
    ivec2 cp = getPosContent();
    if (cs.y <= 0 || cs.x <= 0) {
        return;
    }

    if (trackMixers.isVisible()) {
        nvgSave(vg);
        nvgIntersectScissor(vg, cp.x, cp.y, cs.x, cs.y);
        nvgTranslate(vg, cp.x, cp.y);
        trackMixers.render(vg);
        nvgRestore(vg);
    }

    if (scrollbar.isVisible()) {
        nvgSave(vg);
        nvgTranslate(vg, cp.x, cp.y);
        scrollbar.render(vg);
        nvgRestore(vg);
    }
}

void guictr_mixers::layout() {
    const int32_t trackControlsWidth = theme->get(GuiConstant::CONST_TRACK_CONTROLS_WIDTH);

    int scrollW         = gui_scrollbar::defaultW;

    ivec2 cs       = getSizeContent();
    cs.x = math::max(scrollW+trackControlsWidth+5, cs.x);
    cs.y = math::max(64, cs.y);
    scrollbar.pos  = ivec2(0, cs.y - scrollW);
    scrollbar.size = ivec2(cs.x, scrollW);
    trackMixers.size = cs;
    trackMixers.pos  = ivec2(0, 0);

    ivec2 csTrackView = trackMixers.getSizeContent();

    // Calculate the combined width of all top tracks
    int32_t allTracksWidth = 0;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            allTracksWidth += getTrackTotalWidth(entry);
            allTracksWidth += TRACK_HEIGHT_SPACING;
        }
    }

    // Calculate the x position of the first return
    int32_t xPosFirstReturn = csTrackView.x - TRACK_HEIGHT_SPACING;
    auto itMastersTracks    = guiMgr.trackEntriesBottom.rbegin();
    auto itMastersEnd       = guiMgr.trackEntriesBottom.rend();
    while (itMastersTracks != itMastersEnd) {
        auto& entry = *itMastersTracks;
        if (guiMgr.isVisible(entry)) {
            xPosFirstReturn -= getTrackTotalWidth(entry);
            xPosFirstReturn -= TRACK_HEIGHT_SPACING;
        }
        itMastersTracks++;
    }
    contentWidth    = allTracksWidth;
    contentViewSize = xPosFirstReturn;
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
    contentWidth += TRACK_HEIGHT_STEP * 4;
    scrollbar.setVisible(contentWidth >= contentViewSize);
    if (scrollbar.isVisible()) {
        trackMixers.size.y -= scrollW;
    }

    int32_t scrOffset = math::max(0.0f, getScrollOffset() * (allTracksWidth - contentViewSize));

    int x = TRACK_HEIGHT_SPACING - scrOffset;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            int32_t w = setTrackPosition(entry, x, false);
            x += w + TRACK_HEIGHT_SPACING;
        } else {
            dbgassert(0);
        }
    }


    x = csTrackView.x - TRACK_HEIGHT_SPACING;

    itMastersTracks = guiMgr.trackEntriesBottom.rbegin();
    itMastersEnd    = guiMgr.trackEntriesBottom.rend();
    while (itMastersTracks != itMastersEnd) {
        auto& entry = *itMastersTracks;
        if (guiMgr.isVisible(entry)) {
            dbgassert(entry->trackMixers);
            int32_t w = setTrackPosition(entry, x, true);
            x -= w;
            x -= TRACK_HEIGHT_SPACING;
        } else {
            dbgassert(0);
        }


        itMastersTracks++;
    }

    for (guibase* gui : guis) {
        gui->layout();
    }
}

void guictr_mixers::updateVisibleTracks() {
    guiMgr.updateVisibleTracks(project.trackList);

    track_gui_vector_td& tracks = guiMgr.tracksVisibleFlat;
    for (track_t* tr : project.trackList) {
        track_gui_entry_t* entry = nullptr;
        if (!(guiMgr.getPointerEntry(tr, &entry))) {
            continue;
        }
        if (!assert_expr(entry->trackMixers)) {
            continue;
        }
        const bool bVisible = STL_CONTAINS(tracks, entry);
        entry->trackMixers->setVisible(bVisible);
    }
}


void guictr_mixers::onChildLayoutChanged(guibase* g) {
    layout();
}

bool guictr_mixers::handleEditorCommand(DAW::UI::CommandContext& ctxt) {
    // return trackMixers.handleEditorCommand(ctxt);
    return false;
}

void guictr_mixers::scrollOffsetChanged(int dir, float offset) {
    int32_t scrOffset = math::max(0.0f, offset * (contentWidth - contentViewSize));

    int x = TRACK_HEIGHT_SPACING - scrOffset;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            int32_t w = setTrackPosition(entry, x, false);
            x += w + TRACK_HEIGHT_SPACING;
        }
    }
}

bool guictr_mixers::handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) {
    return scrollbar.handleMouseScroll(evt, xoffset, yoffset);
}

void guictr_mixers::scrollTo(guibase* g) {
    int32_t scrOffset = math::max(0.0f, scrollbar.scrollOffset * (contentWidth - contentViewSize));

    int32_t x = g->pos.x;
    scrollbar.scrollVisible(x + scrOffset, g->size.y);
}

void guictr_mixers::onAdded() {
    guictr_base::onAdded();
    removeAllTracks();
    for (track_t* tr : project.trackList) {
        removeTrack(tr, FLG_TRK_CHANGE_LOAD);
    }
    addAllTracks();
}

void guictr_mixers::onRemove() {
    removeAllTracks();
    guictr_base::onRemove();
}

void guictr_mixers::addAllTracks() {
    for (track_t* tr : project.trackList) {
        addTrack(tr, FLG_TRK_CHANGE_LOAD);
    }
}

void guictr_mixers::removeAllTracks() {
    track_gui_vector_td tracksCopy = guiMgr.getTracksVisibleFlat();
    for (auto* entry : tracksCopy) {
        removeTrack(entry->track, FLG_TRK_CHANGE_LOAD);
    }
}

void guictr_mixers::removeTrack(track_t* track, int flags) {
    track_gui_entry_t* entry = nullptr;
    if (!guiMgr.getPointerEntry(track, &entry)) {
        return;
    }
    dbgassert(track->audio);
    trackMixers.removeTrackEntry(*entry);
    // removeEntry(track->audio->guiInstances, entry);
    dbgassert(entry->trackMixers);
    delete entry->trackMixers;
    entry->trackMixers = nullptr;
    guiMgr.removeTrack(*entry); // does delete entry
}

void guictr_mixers::addTrack(track_t* track, int flags) {
    dbgassert(track->audio);
    auto* entry = new track_gui_entry_t{};

    entry->parentCtrl = this->dawCtrl;
    entry->track      = track;
    entry->parent     = nullptr;
    // dbgassert(0);
    entry->trackMixers = DAW::createTrackGuiMixer(entry);
    entry->trackMixers->id = track->localIdxFlat;

    guiMgr.addTrack(entry);
    trackMixers.addTrackEntry(*entry);
    // track->audio->guiInstances.push_back(entry);

    //TODO: restore subtracks
    if (!(flags & FLG_TRK_CHANGE_LOAD)) {
        updateVisibleTracks();
        layout();
    }
}

void guictr_mixers::resetView() {
    guiMgr.reset();
}

void guictr_mixers::guictr_mixers_content::addTrackEntry(track_gui_entry_t& e) {
    this->add(e.trackMixers);
}

void guictr_mixers::guictr_mixers_content::removeTrackEntry(track_gui_entry_t& e) {
    this->remove(e.trackMixers);
    if (dawCtrl) {
        dawCtrl->onTrackMixerRemoved(e);
    }
}