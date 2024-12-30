#include <nanovg.h>
#include "assert_dbg.h"
#include "gui/container/container.h"
#include "gui/gui.h"
#include "gui/meter/guimeter.h"
#include "guiconstant.h"
#include "logging.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "trackmixers.h"

namespace DAW {
    guictr_base* createTrackGuiMixer(guictr_mixers* mixer, track_gui_entry_t* entry);
}

guictr_mixers::guictr_mixers(DawCtrl* _dawCtrl, DAW::Cursor& _cursor, DAW::TrackSelection& _trackSelection, project_t& _project, project_globals_t& _projectGlobals, dragdrop_file& _dragdropclip)
    : guictr_base(),
      trackMixerGlobalIndex(_dawCtrl->getDawWindowIndex()),
      project(_project),
      projectGlobals(_projectGlobals),
      guiMgr(),
      scrollbar(0, 0.0f, *this),
      trackMixers(),
      mixerOptions(this)
{
    setGuiType(gui_type::CTR_TYPE_MIXERS);
    padding = 0;
    margin  = 0;
    dawCtrl = _dawCtrl,
    setCanMouseHit(true);
    setBackgroundRendered(true);
    setCanMouseHit(false);
    setBackgroundRendered(false);
    add(&trackMixers);
    add(&mixerOptions);
    add(&scrollbar);
}

guictr_mixers::~guictr_mixers() {
    remove(&scrollbar);
    remove(&mixerOptions);
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

    if (mixerOptions.isVisible()) {
        nvgSave(vg);
        nvgTranslate(vg, cp.x, cp.y);
        mixerOptions.render(vg);
        nvgRestore(vg);
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
    const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

    int scrollW = gui_scrollbar::defaultW;
    ivec2 cs    = getSizeContent();
    cs.x = math::max(scrollW+trackControlsWidth+5, cs.x);
    cs.y = math::max(TRACK_HEIGHT_STEP, cs.y);
    scrollbar.pos  = ivec2(0, cs.y - scrollW);
    scrollbar.size = ivec2(cs.x, scrollW);
    mixerOptions.size = ivec2(TRACK_HEIGHT_STEP, cs.y);
    mixerOptions.pos  = ivec2(cs.x - mixerOptions.size.x, 0);
    trackMixers.size = cs - ivec2(mixerOptions.size.x, 0);
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
    scrollbar.setVisible(allTracksWidth >= xPosFirstReturn);
    contentWidth    = allTracksWidth;
    contentViewSize = xPosFirstReturn;
    contentWidth += TRACK_HEIGHT_STEP * 4;
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
    entry->trackMixers = DAW::createTrackGuiMixer(this, entry);
    entry->trackMixers->id = track->localIdxFlat;
    entry->layout.height = bWideLayout ? 10 : 4;

    guiMgr.addTrack(entry);
    trackMixers.addTrackEntry(*entry);
    // track->audio->guiInstances.push_back(entry);

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

namespace DAW {
    guictr_base* createTrackControlsIO(track_gui_entry_t* _entry);

    class guictr_mixers_mixer : public guictr_base {
        guictr_mixers* const m_parent;
        track_t* const m_track;
        track_gui_entry_t* const m_trackentry;
        DAW::rmsmeter m_subMeter;
        gui_trackmeter m_guiMeter;
        gui_slider_gain_vertical trackGain;
        gui_slider_pan trackPanning;
        guibutton_trackbypass btnBypass;
        guibutton_track_solo btnSolo;
        guibutton_track_record_arm btnRecord;
        guibutton btnActivate;
        std::vector<gui_slider_gain*> sendGains;
        std::vector<gui_slider_pan*> sendPans;
        guictr_base* trackIO;
    public:
        guictr_mixers_mixer(guictr_mixers* _mixer, track_gui_entry_t* _entry) 
            : guictr_base(),
              m_parent(_mixer),
              m_track(_entry->track),
              m_trackentry(_entry),
              m_subMeter(_entry->track->audio->meter.getSubChannelMeter(0, 2)),
              m_guiMeter(&m_subMeter),
              trackGain(_entry),
              btnBypass(_entry),
              btnSolo(_entry) ,
              btnRecord(_entry),
              trackIO(createTrackControlsIO(_entry))
        {
            (void) m_parent;
            (void) m_trackentry;
            padding = 2;
            margin  = 2;
            setBackgroundRendered(true);
            setCanMouseHit(true);
            trackGain.setAutomationRef(&m_track->audio->mixer, PARAM_TRACK_GAIN);
            trackPanning.setAutomationRef(&m_track->audio->mixer, PARAM_TRACK_PAN);
            padding            = 0;
            btnBypass.drawFn   = drawTextureSymbol;
            btnBypass.drawParm = ICON_BYPASS;
            btnBypass.setFlag(FLG_RENDER_BUTTON_WITH_LED, true);
            btnActivate.drawFn   = drawTextureSymbol;
            btnActivate.drawParm = ICON_EFFECT;
            trackGain.setLabel("Gain Level");
            trackPanning.setLabel("Pan");
            btnActivate.setLabel("Load plugins");
            add(&btnBypass);
            add(&btnSolo);
            add(&btnRecord);
            add(&btnActivate);
            add(&trackGain);
            add(&trackPanning);
            add(&m_guiMeter);
            add(trackIO);
            trackPanning.setFlag(FLG_RENDER_LABEL, false);
            if (m_track->type != TRACK_TYPE_MASTER && m_track->type != TRACK_TYPE_RETURN) {
                sendGains.resize(MAX_SEND_CHANNELS);
                sendPans.resize(MAX_SEND_CHANNELS);
                for (int i = 0; i < MAX_SEND_CHANNELS; i++) {
                    sendGains[i] = new gui_slider_gain();
                    sendGains[i]->setVisible(false);
                    sendGains[i]->setAutomationRef(&m_track->audio->mixer, PARAM_OFFSET_SEND_GAIN + i);
                    sendGains[i]->setLabel(StringFormat("Send %d", i + 1));
                    sendGains[i]->setFlag(FLG_RENDER_LABEL, true);
                    sendPans[i] = new gui_slider_pan();
                    sendPans[i]->setVisible(false);
                    sendPans[i]->setAutomationRef(&m_track->audio->mixer, PARAM_OFFSET_SEND_PAN + i);
                    sendPans[i]->setLabel("Pan");
                    sendGains[i]->setFlag(FLG_RENDER_LABEL, false);
                    sendPans[i]->setFlag(FLG_RENDER_LABEL, false);
                    add(sendGains[i]);
                    add(sendPans[i]);
                }
            }
        }
        ~guictr_mixers_mixer() override {
            for (auto* sendGainCtrl : sendGains) {
                remove(sendGainCtrl);
                delete sendGainCtrl;
            }
            for (auto* sendPanCtrl : sendPans) {
                remove(sendPanCtrl);
                delete sendPanCtrl;
            }
            remove(trackIO);
            delete trackIO;
            remove(&m_guiMeter);
            remove(&trackPanning);
            remove(&trackGain);
            remove(&btnActivate);
            remove(&btnRecord);
            remove(&btnSolo);
            remove(&btnBypass);
        }
        void render(NVGcontext* vg) override {
            if (!setScissorTransform(vg)) {
                return;
            }
            auto cs = getSizeContent();
            auto trackRgb = rgbToNvg(m_track->rgb);
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, cs.x, cs.y);
            nvgFillColor(vg, trackRgb);
            nvgFill(vg);
            for (auto gui : guis) {
                if (gui->isVisible()) {
                    nvgSave(vg);
                    gui->render(vg);
                    nvgRestore(vg);
                }
            }
        }
        void buttonClicked(guibase* button) override {
            auto const daw = dawCtrl->getDaw();
            ThreadLock lock = daw->lockPlayThread();
            if (&btnSolo == button) {
                bool isSolo = (m_track->audio->flags & audiostageflags_t::SOLO) != audiostageflags_t::NONE;
                if (!isShift(parentCtrl->lastMouseEvent.kbmods)) {
                    daw->unsoloAll();
                }
                daw->setSoloState(m_track->audio->toRef(), !isSolo);
            }
            if (&btnRecord == button) {
                bool isArmed = (m_track->audio->flags & audiostageflags_t::RECORD_ARMED) != audiostageflags_t::NONE;
                daw->setTrackArmed(m_track->audio->toRef(), !isArmed);
            }
            if (&btnBypass == button) {
                track_params_t& trackParams = m_track->audio->mixer;
                auto fNew = float(!trackParams.isEnabled());
                auto flags = FLG_PAR_UPDATE_FINISH | FLG_PAR_UPDATE_USER;
                trackParams.setParamEdit(PARAM_ENABLE, fNew, flags);
            }
            if (&btnActivate == button) {
                auto pluginMgr = daw->getPluginManager();
                std::vector<effectbase*> effects;
                m_track->audio->getDeferredEffects(effects);
                for (auto effect : effects) {
                    pluginMgr->activateDeferred(effect, 0);
                }
                daw->onPluginsChanged();
                onChildLayoutChanged(button);
    
    #ifndef NDEBUG
                log_printf("deferredEffects post activateDeferred on track %s: %zu\n", m_track->szName, m_track->audio->deferredEffects.size());
    #endif
            }
        }
        void onTick(AppCtrl* ctrl) override {
            for (guibase* gui : guis) {
                if (gui->isVisible()) {
                    gui->onTick(ctrl);
                }
            }
        }
        void layout() override {
            std::vector<effectbase*> effects;
            dbgassert(m_track->audio);
            m_track->audio->getDeferredEffects(effects);
            int nDefEffects = CtrSize(effects);
            btnActivate.setEnabled(nDefEffects > 0);
            auto str = nDefEffects > 9 ? "9+" : (StringFormat("%d", nDefEffects));
            // btnActivate.setText(str);
            btnActivate.setLabel("Load "+str+" deferred plugins");
            btnActivate.setVisible(nDefEffects > 0);
    
            const int32_t CONST_PADDING_TRACK_CONTROLS = theme->get(GuiConstant::CONST_PADDING_TRACK_CONTROLS);
            const int32_t TRACK_HEIGHT_STEP   = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
            const int32_t mW = m_parent->bWideLayout ? size.x / 2 : 24;
    
            int32_t inset = CONST_PADDING_TRACK_CONTROLS;
            int32_t i2    = inset * 2;
            m_guiMeter.size = ivec2(mW - i2, size.y - i2);
            m_guiMeter.pos  = ivec2(size.x - mW + inset, inset);
    
    
            int32_t heightInner = TRACK_HEIGHT_STEP - i2;
            int32_t csX      = size.x - mW;
            int32_t csY      = size.y;
            int32_t posY = 0;
            guibase* lastGui = nullptr;
            if (trackIO) {
                trackIO->setVisible(m_parent->bShowIO && m_parent->bWideLayout);
                if (trackIO->isVisible()) {
                    trackIO->pos = ivec2(inset, posY + inset);
                    trackIO->size = ivec2(csX - i2, TRACK_HEIGHT_STEP*3 - i2);
                    posY += TRACK_HEIGHT_STEP*3;
                    lastGui = trackIO;
                }
            }
            if (!sendGains.empty()) {
                project_t* project = dawCtrl->getDaw()->getProject();
                dbgassert(project);
                int32_t numReturnChannels = project->trackReturnCtr.size();
                bool bShowPan = m_parent->bWideLayout;
                if (!m_parent->bShowSends) {
                    numReturnChannels = 0;
                }
                float sendGainWidth = (csX);
                for (int32_t i = 0; i < numReturnChannels; ++i) {
                    auto wGain = bShowPan ? sendGainWidth*3/5 : sendGainWidth;
                    sendGains[i]->size = ivec2(wGain - i2, heightInner);
                    sendGains[i]->pos  = ivec2(inset, posY + inset);
                    if (bShowPan) {
                        sendPans[i]->size = ivec2(sendGainWidth*2/5 - i2, heightInner);
                        sendPans[i]->pos  = ivec2(inset + sendGainWidth*3/5, posY + inset);
                    }
                    posY += TRACK_HEIGHT_STEP;
                    lastGui = sendGains[i];
                }
                for (auto sendGainCtrl : sendGains) {
                    auto idx = sendGainCtrl->getParamIdx() - PARAM_OFFSET_SEND_GAIN;
                    sendGainCtrl->setVisible(idx < numReturnChannels);
                }
                for (auto sendPanCtrl : sendPans) {
                    auto idx = sendPanCtrl->getParamIdx() - PARAM_OFFSET_SEND_PAN;
                    sendPanCtrl->setVisible(bShowPan && idx < numReturnChannels);
                }
            }
            int32_t posYGain = posY;
    
            posY = csY - (btnActivate.isVisible() ? 3 : 2) * TRACK_HEIGHT_STEP;
    
            auto btnSize = csX/2;
            btnSolo.pos      = ivec2(inset, posY + inset);
            btnSolo.size     = ivec2(btnSize - i2, heightInner);
    
            btnRecord.pos    = ivec2(csX / 2 + inset, posY + inset);
            btnRecord.size   = ivec2(btnSize - i2, heightInner);
            posY += TRACK_HEIGHT_STEP;
    
            if (btnActivate.isVisible()) {
                btnActivate.pos    = ivec2(inset, posY + inset);
                btnActivate.size   = ivec2(csX - i2, heightInner);
                posY += TRACK_HEIGHT_STEP;
            }
    
            btnBypass.size   = ivec2(csX - i2, heightInner);
            btnBypass.pos    = ivec2(inset, posY + inset);
    
            trackGain.pos      = ivec2(inset, posYGain + inset);
            auto lastGuiBottom = lastGui ? lastGui->bottom() : 0;
            trackGain.size     = ivec2(csX - i2, ((btnSolo.top() - lastGuiBottom) - i2) - i2 - TRACK_HEIGHT_STEP);
    
            trackPanning.pos   = ivec2(inset, trackGain.bottom() + i2);
            trackPanning.size  = ivec2(csX - i2, heightInner);
    
            for (auto gui : guis) {
                gui->layout();
            }
        }
    };

    guictr_base* createTrackGuiMixer(guictr_mixers* mixer, track_gui_entry_t* entry) {
        return new guictr_mixers_mixer(mixer, entry);
    }
}// namespace DAW
