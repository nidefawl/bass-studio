#include <nanovg.h>
#include "assert_dbg.h"
#include "gui/container/container.h"
#include "gui/gui.h"
#include "gui/meter/guimeter.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "logging.h"
#include "math/seq_math.h"
#include "math/vec.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "trackmixers.h"

namespace DAW {
    guictr_base* createTrackGuiMixer(guictr_mixers* mixer, track_gui_entry_t* entry);
    guictr_base* createTrackGuiMixerTitle(guictr_mixers* mixer, track_gui_entry_t* entry);
}

namespace
{
    constexpr int32_t TRACK_MIXER_SPACING = 4;
    constexpr int32_t TRACK_MIXER_TITLE_HEIGHT = 24;
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
    const int32_t MIXER_SIZE_STEP = theme->get(GuiConstant::CONST_MIXER_SIZE_STEP);
    int32_t trH = e->getHeight();
    int32_t totalHeight = trH * MIXER_SIZE_STEP;
    return totalHeight;
}

int32_t guictr_mixers::setTrackPosition(track_gui_entry_t* e, int32_t x, bool isBottom) {
    int32_t childTrackInsetY = (e->track->getChildLvl() + 1) * TRACK_MIXER_TITLE_HEIGHT;
    const int32_t MIXER_SIZE_STEP = theme->get(GuiConstant::CONST_MIXER_SIZE_STEP);
    int32_t trH      = e->getHeight();
    e->trackMixer->size = ivec2(trH * MIXER_SIZE_STEP, trackMixers.size.y);
    if (isBottom) {
        e->trackMixer->pos  = ivec2(x - e->trackMixer->size.x , 0);
    } else {
        e->trackMixer->pos  = ivec2(x, 0);
    }
    e->trackMixer->pos.y += childTrackInsetY;
    e->trackMixer->size.y -= childTrackInsetY;
    e->trackMixerTitle->pos = e->trackMixer->pos - ivec2(0, TRACK_MIXER_TITLE_HEIGHT);
    e->trackMixerTitle->size = ivec2(e->trackMixer->size.x, TRACK_MIXER_TITLE_HEIGHT);
    return e->trackMixer->size.x;
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
    const int32_t MIXER_SIZE_STEP = theme->get(GuiConstant::CONST_MIXER_SIZE_STEP);

    int scrollW = gui_scrollbar::defaultW;
    ivec2 cs    = getSizeContent();
    cs.x = math::max(scrollW+trackControlsWidth+5, cs.x);
    cs.y = math::max(MIXER_SIZE_STEP, cs.y);
    scrollbar.pos  = ivec2(0, cs.y - scrollW);
    scrollbar.size = ivec2(cs.x, scrollW);
    mixerOptions.size = ivec2(MIXER_SIZE_STEP, cs.y);
    mixerOptions.pos  = ivec2(cs.x - mixerOptions.size.x, 0);
    trackMixers.size = cs - ivec2(mixerOptions.size.x, 0);
    trackMixers.pos  = ivec2(0, 0);

    ivec2 csTrackView = trackMixers.getSizeContent();

    // Calculate the combined width of all top tracks
    int32_t allTracksWidth = 0;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            allTracksWidth += getTrackTotalWidth(entry);
            allTracksWidth += TRACK_MIXER_SPACING;
        }
    }

    // Calculate the x position of the first return
    int32_t xPosFirstReturn = csTrackView.x - TRACK_MIXER_SPACING;
    auto itMastersTracks    = guiMgr.trackEntriesBottom.rbegin();
    auto itMastersEnd       = guiMgr.trackEntriesBottom.rend();
    while (itMastersTracks != itMastersEnd) {
        auto& entry = *itMastersTracks;
        if (guiMgr.isVisible(entry)) {
            xPosFirstReturn -= getTrackTotalWidth(entry);
            xPosFirstReturn -= TRACK_MIXER_SPACING;
        }
        itMastersTracks++;
    }
    scrollbar.setVisible(allTracksWidth >= xPosFirstReturn);
    contentWidth    = allTracksWidth;
    contentViewSize = xPosFirstReturn;
    contentWidth += MIXER_SIZE_STEP * 4;
    if (scrollbar.isVisible()) {
        trackMixers.size.y -= scrollW;
    }

    int32_t scrOffset = math::max(0.0f, getScrollOffset() * (allTracksWidth - contentViewSize));

    int x = TRACK_MIXER_SPACING - scrOffset;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            int32_t w = setTrackPosition(entry, x, false);
            x += w + TRACK_MIXER_SPACING;
        } else {
            dbgassert(0);
        }
    }


    x = csTrackView.x - TRACK_MIXER_SPACING;

    itMastersTracks = guiMgr.trackEntriesBottom.rbegin();
    itMastersEnd    = guiMgr.trackEntriesBottom.rend();
    while (itMastersTracks != itMastersEnd) {
        auto& entry = *itMastersTracks;
        if (!assert_expr(entry->trackMixer)) {
            continue;
        }
        if (guiMgr.isVisible(entry)) {
            int32_t w = setTrackPosition(entry, x, true);
            x -= w;
            x -= TRACK_MIXER_SPACING;
        } else {
            dbgassert(0);
        }
        itMastersTracks++;
    }
    auto itAll = guiMgr.tracksVisibleFlat.begin();
    auto itEnd = guiMgr.tracksVisibleFlat.end();
    for (; itAll != itEnd; ++itAll) {
        auto* entry = *itAll;
        auto itNext = itAll;
        for (++itNext; itNext != itEnd; ++itNext) {
            auto* entryNext = *itNext;
            if (!entryNext->track->isChildOf(entry->track)) {
                break;
            }
            entry->trackMixerTitle->size.x = entryNext->trackMixerTitle->right() - entry->trackMixerTitle->left();
        }
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
        if (!assert_expr(entry->trackMixer)) {
            continue;
        }
        const bool bVisible = STL_CONTAINS(tracks, entry);
        entry->trackMixer->setVisible(bVisible);
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

    int x = TRACK_MIXER_SPACING - scrOffset;
    for (auto* entry : guiMgr.trackEntriesTop) {
        if (guiMgr.isVisible(entry)) {
            int32_t w = setTrackPosition(entry, x, false);
            x += w + TRACK_MIXER_SPACING;
        }
    }
    auto itAll = guiMgr.tracksVisibleFlat.begin();
    auto itEnd = guiMgr.tracksVisibleFlat.end();
    for (; itAll != itEnd; ++itAll) {
        auto* entry = *itAll;
        auto itNext = itAll;
        for (++itNext; itNext != itEnd; ++itNext) {
            auto* entryNext = *itNext;
            if (!entryNext->track->isChildOf(entry->track)) {
                break;
            }
            entry->trackMixerTitle->size.x = entryNext->trackMixerTitle->right() - entry->trackMixerTitle->left();
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
    dbgassert(entry->trackMixer);
    delete entry->trackMixer;
    entry->trackMixer = nullptr;
    dbgassert(entry->trackMixerTitle);
    delete entry->trackMixerTitle;
    entry->trackMixerTitle = nullptr;
    guiMgr.removeTrack(*entry); // does delete entry
}

void guictr_mixers::addTrack(track_t* track, int flags) {
    dbgassert(track->audio);
    auto* entry = new track_gui_entry_t{};

    entry->parentCtrl = this->dawCtrl;
    entry->track      = track;
    entry->parent = dawCtrl->getTrackContainer().get();
    entry->trackMixer = DAW::createTrackGuiMixer(this, entry);
    entry->trackMixerTitle = DAW::createTrackGuiMixerTitle(this, entry);
    entry->trackMixer->id = track->localIdxFlat;
    entry->trackMixerTitle->id = track->localIdxFlat;
    entry->layout.height = bWideLayout ? 6 : 4;

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
    this->add(e.trackMixer);
    this->add(e.trackMixerTitle);
}

void guictr_mixers::guictr_mixers_content::removeTrackEntry(track_gui_entry_t& e) {
    this->remove(e.trackMixer);
    this->remove(e.trackMixerTitle);
    if (dawCtrl) {
        dawCtrl->onTrackMixerRemoved(e);
    }
}
void guictr_mixers::handleRightClick(MouseEvent& evt) {
    if (!assert_expr(dawCtrl)) {
        return;
    }
    parentCtrl->openContextMenu(new guictxtmenu_notrack(dawCtrl), evt.mousepos);
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
            padding = 0;
            margin  = 0;
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
            trackPanning.setFlag(FLG_RENDER_LABEL, true);
            trackGain.setFlag(FLG_RENDER_LABEL, false);
            if (m_track->type != TRACK_TYPE_MASTER && m_track->type != TRACK_TYPE_RETURN) {
                sendGains.resize(MAX_SEND_CHANNELS);
                sendPans.resize(MAX_SEND_CHANNELS);
                for (int i = 0; i < MAX_SEND_CHANNELS; i++) {
                    sendGains[i] = new gui_slider_gain();
                    sendGains[i]->setVisible(false);
                    sendGains[i]->setAutomationRef(&m_track->audio->mixer, PARAM_OFFSET_SEND_GAIN + i);
                    sendGains[i]->setLabel(StringFormat("Send %d", i + 1));
                    sendPans[i] = new gui_slider_pan();
                    sendPans[i]->setVisible(false);
                    sendPans[i]->setAutomationRef(&m_track->audio->mixer, PARAM_OFFSET_SEND_PAN + i);
                    sendPans[i]->setLabel("Pan");
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
        void renderGroupHandles(NVGcontext* vg) {
            auto lvl = (m_track->getChildLvl() + 1);
            auto p   = m_track;
            if (dawCtrl->getSelectedTrack() == m_track) {
                NVGcolor color2 = theme->getColor(GuiColor::COL_BG_SELECTEDTRACK_TITLE);
                nvgBeginPath(vg);
                nvgRect(vg, pos.x, pos.y, size.x, size.y);
                nvgFillColor(vg, color2);
                nvgFill(vg);
            }
            while (p) {
                dbgassert(lvl);

                int32_t insetX        = 0;
                int32_t insetY        = 2;
                const int titleHeight = TRACK_MIXER_TITLE_HEIGHT;
                nvgBeginPath(vg);
                vec2 titlePos = vec2(pos.x, (lvl - 1) * titleHeight);
                vec2 titleSize = vec2(size.x, titleHeight);
                nvgRect(vg, titlePos.x + insetX, titlePos.y + insetY, titleSize.x - insetX * 2, titleSize.y - insetY * 2);
                nvgFillColor(vg, rgbToNvg(p->rgb));
                nvgFill(vg);
                if (p == m_track) {
                    NVGcolor color = rgbToNvg(m_track->rgb);
                    renderTextLabel(vg,
                        titlePos + vec2(2, titleHeight / 2),
                        titleSize,
                        m_track->name,
                        theme,
                        titleHeight - 4,
                        getContrastFontColorNvg(color),
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                }
                p = p->parent;
                lvl--;
            }
        }
        void render(NVGcontext* vg) override {
            if (isBackgroundRendered()) {
                renderBackground(vg);
            }
            if (!setScissorTransform(vg)) {
                return;
            }
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
            btnActivate.setLabel("Load "+str+" deferred plugins");
            btnActivate.setVisible(nDefEffects > 0);
    
            const int32_t CONST_PADDING_TRACK_CONTROLS = theme->get(GuiConstant::CONST_PADDING_TRACK_CONTROLS);
            const int32_t MIXER_SIZE_STEP   = theme->get(GuiConstant::CONST_MIXER_SIZE_STEP);
    
            int32_t inset = CONST_PADDING_TRACK_CONTROLS;
            int32_t i2    = inset * 2;
    
    
            int32_t heightInner = MIXER_SIZE_STEP - i2;
            int32_t csX      = size.x;
            int32_t csY      = size.y;
            int32_t posY = 0;
            guibase* lastGui = nullptr;
            if (trackIO) {
                trackIO->setVisible(m_parent->bShowIO && m_parent->bWideLayout);
                if (trackIO->isVisible()) {
                    trackIO->pos = ivec2(inset, posY + inset);
                    trackIO->size = ivec2(csX - i2, MIXER_SIZE_STEP*3 - i2);
                    posY += MIXER_SIZE_STEP*3;
                    lastGui = trackIO;
                }
            }
            if (!sendGains.empty()) {
                project_t* project = dawCtrl->getDaw()->getProject();
                dbgassert(project);
                int32_t numReturnChannels = project->trackReturnCtr.size();
                bool bShowPan = m_trackentry->layout.height > 3;
                if (!m_parent->bShowSends) {
                    numReturnChannels = 0;
                }
                float sendGainWidth = (csX);
                for (int32_t i = 0; i < numReturnChannels; ++i) {
                    auto wGain = bShowPan ? sendGainWidth*3/4 : sendGainWidth;
                    sendGains[i]->size = ivec2(wGain - i2, heightInner);
                    sendGains[i]->pos  = ivec2(inset, posY + inset);
                    if (bShowPan) {
                        sendPans[i]->size = ivec2(sendGainWidth*1/4 - i2, heightInner);
                        sendPans[i]->pos  = ivec2(inset + sendGainWidth*3/4, posY + inset);
                    }
                    posY += MIXER_SIZE_STEP;
                    lastGui = sendGains[i];
                }
                for (auto sendGainCtrl : sendGains) {
                    auto idx = sendGainCtrl->getParamIdx() - PARAM_OFFSET_SEND_GAIN;
                    sendGainCtrl->setFlag(FLG_RENDER_LABEL, m_trackentry->layout.height > 4);
                    sendGainCtrl->setVisible(idx < numReturnChannels);
                }
                for (auto sendPanCtrl : sendPans) {
                    auto idx = sendPanCtrl->getParamIdx() - PARAM_OFFSET_SEND_PAN;
                    sendPanCtrl->setFlag(FLG_RENDER_LABEL, m_trackentry->layout.height > 4);
                    sendPanCtrl->setVisible(bShowPan && idx < numReturnChannels);
                }
            }
    
            posY = csY - (btnActivate.isVisible() ? 4 : 3) * MIXER_SIZE_STEP;
    
            auto btnSize = csX/2;
            trackPanning.pos   = ivec2(inset, posY + inset);
            trackPanning.size  = ivec2(csX - i2, heightInner);
            trackPanning.setFlag(FLG_RENDER_LABEL, m_trackentry->layout.height > 4);
            posY += MIXER_SIZE_STEP;


            btnSolo.pos      = ivec2(inset, posY + inset);
            btnSolo.size     = ivec2(btnSize - i2, heightInner);
    
            btnRecord.pos    = ivec2(csX / 2 + inset, posY + inset);
            btnRecord.size   = ivec2(btnSize - i2, heightInner);
            posY += MIXER_SIZE_STEP;
    
            if (btnActivate.isVisible()) {
                btnActivate.pos    = ivec2(inset, posY + inset);
                btnActivate.size   = ivec2(csX - i2, heightInner);
                posY += MIXER_SIZE_STEP;
            }
    
            btnBypass.size   = ivec2(csX - i2, heightInner);
            btnBypass.pos    = ivec2(inset, posY + inset);
    
            auto trackGainWidth = csX * 1 / 2;
            auto lastGuiBottom = lastGui ? lastGui->bottom() : 0;
            trackGain.pos.x = 0;
            trackGain.pos.y = lastGuiBottom;
            trackGain.size     = ivec2(trackGainWidth, ((trackPanning.top() - lastGuiBottom) - i2));
            auto maxWidth = math::roundfS32((m_trackentry->getHeight() * 0.5f * MIXER_SIZE_STEP) * 0.8f);
            if (trackGainWidth > maxWidth) {
                trackGain.pos.x = (trackGain.size.x - maxWidth) / 2;
                trackGain.size.x = maxWidth;
            }
            trackGain.pos.y += i2;
            trackGain.size.y -= i2;
            m_guiMeter.size = trackGain.size;
            m_guiMeter.pos.y = trackGain.pos.y;
            m_guiMeter.pos.x = trackGainWidth + ((csX - trackGainWidth) - m_guiMeter.size.x) / 2;
    
            for (auto gui : guis) {
                gui->layout();
            }
        }

        void handleDraggedBegin(MouseEvent& evt) override {
            dawCtrl->setSelectedTrack(this->m_track);
        }
        bool focusEvent(MouseHitEvt& evt, bool focused) override {
            if (focused) {
                dawCtrl->setSelectedTrack(this->m_track);
            }
            return true;
        }
        void handleDraggedMove(MouseEvent& evt) override {
            parentCtrl->objectDragMove(this, evt);
        }
        
        void handleDraggedRelease(MouseEvent& evt) override {
            parentCtrl->objectDragRelease(this, evt);
        }
        
        void dragMoveOn(guibase* target, ivec2 mousepos) override {
            target->trackEntryDragMove(this->m_trackentry, toControlsObjectSpace(mousepos, target));
        }
        
        void dragReleaseOn(guibase* target, ivec2 mousepos) override {
            target->trackEntryDragRelease(this->m_trackentry, toControlsObjectSpace(mousepos, target));
        }
        void handleRightClick(MouseEvent& evt) override {
            auto trackCtr = dawCtrl->getTrackContainer();
            if (!trackCtr)
                return;
            track_gui_entry_t* entryTrackCtr = nullptr;
            if (!trackCtr->getTrackEntry(m_track, &entryTrackCtr))
                return;
            m_trackentry->parentCtrl->openContextMenu(new guictxtmenu_track(dawCtrl, entryTrackCtr), evt.mousepos);
        }
    };

    guictr_base* createTrackGuiMixer(guictr_mixers* mixer, track_gui_entry_t* entry) {
        return new guictr_mixers_mixer(mixer, entry);
    }

    class guictr_mixertitle final : public guictr_base {

        track_t* const m_track;
        track_gui_entry_t* const m_trackentry;
        DragModeTrack dragMode = DragModeTrack::DRAG_TRACK_NONE;
    
    public:
        explicit guictr_mixertitle(guictr_mixers* _mixer, track_gui_entry_t* _entry)
            : guictr_base(),
              m_track(_entry->track),
              m_trackentry(_entry) {
            setGuiType(gui_type::CTR_TYPE_TRACK_TITLE);
            setCanMouseHit(true);
            padding = 1;
        }
        ~guictr_mixertitle() override {
        }
        track_gui_entry_t* getTrackEntry() {
            return m_trackentry;
        }
        const track_gui_entry_t* getTrackEntry() const {
            return m_trackentry;
        }
        bool isResize(ivec2 mpos) {
            int32_t resizeLeftOrRight = m_track->type < TRACK_TYPE_MIDI ? left() : right();
            return mpos.x >= resizeLeftOrRight - DRAG_RANGE/2 && mpos.x < resizeLeftOrRight + DRAG_RANGE/2 && mpos.y >= top() && mpos.y < bottom();
        }
        bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
            if (isResize(mpos)) {
                evt.requestFocus(this);
                if (evt.type <= MouseHitType::MOUSE_RIGHT)
                    evt.requestCursor(CURSOR_RESIZE_H);
                return true;
            }
            if (contains(mpos)) {
                ivec2 local = this->toContainerSpace(mpos);
                for (guibase* gui : guis) {
                    if (gui->isVisible() && gui->mouseHitTest(local, evt)) {
                        return true;
                    }
                }
                evt.requestFocus(this);
                return true;// always need to return true if contained, parent has z-order
            }
            return false;
        }
        void handleDraggedBegin(MouseEvent& evt) override {
            dragMode = DragModeTrack::DRAG_TRACK_NONE;
            if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
                const int titleHeight = TRACK_MIXER_TITLE_HEIGHT;
                if (evt.relMousepos.y < titleHeight)
                    DAW::OpenRenameTrackPopup(dawCtrl, m_trackentry);
                return;
            }
            dawCtrl->setSelectedTrack(m_track);
            if (isResize(evt.relMousepos+getPosContent())) {
                dragMode = DragModeTrack::DRAG_TRACK_RESIZE;
            }
        }
    
        void handleDraggedMove(MouseEvent& evt) override {
            if (dragMode == DragModeTrack::DRAG_TRACK_RESIZE) {
                int32_t mouseDragDist = evt.relMousepos.x;
                const int32_t MIXER_SIZE_STEP = theme->get(GuiConstant::CONST_MIXER_SIZE_STEP);
                parent->onChildLayoutChanged(this);
                auto newHeight = math::min(TRACK_MAX_HEIGHT, math::max(TRACK_MIN_HEIGHT, (mouseDragDist) /  MIXER_SIZE_STEP));
                m_trackentry->layout.height = newHeight;
                dawCtrl->updateVisibleTrackContents();
            } else {
                parentCtrl->objectDragMove(this, evt);
            }
        }
    
        void handleDraggedRelease(MouseEvent& evt) override {
            if (dragMode == DragModeTrack::DRAG_TRACK_RESIZE) {
    
            } else {
                parentCtrl->objectDragRelease(this, evt);
            }
            dragMode = DragModeTrack::DRAG_TRACK_NONE;
        }
        void render(NVGcontext* vg) override {
            if (!setScissorTransform(vg)) {
                return;
            }
            auto cs               = getSizeContent();
            NVGcolor color        = rgbToNvg(m_track->rgb);
            const int titleHeight = TRACK_MIXER_TITLE_HEIGHT;
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, cs.x, cs.y);
            nvgFillColor(vg, color);
            nvgFill(vg);
            bool bIsSelected = dawCtrl->getSelectedTrack() == m_track;
            if (bIsSelected) {
                color = theme->getColor(GuiColor::COL_BG_SELECTEDTRACK);
                nvgBeginPath(vg);
                nvgRect(vg, 0, 3, cs.x, cs.y-3);
                nvgFillColor(vg, color);
                nvgFill(vg);
            }
            NVGcolor color2 = theme->getColor(bIsSelected ? GuiColor::COL_BG_SELECTEDTRACK_TITLE : GuiColor::COL_BG_BRT);
            if (!bIsSelected) {
                color2.a = 0.4f;
            }
            nvgBeginPath(vg);
            nvgRect(vg, 0, 0, cs.x, 3);
            nvgFillColor(vg, color2);
            nvgFill(vg);

            int32_t insetLabel = 1;
            vec2 titlePos      = ivec2(insetLabel * 3, 0);
            vec2 titleSize     = cs - ivec2(insetLabel * 4, 0);
            renderTextLabel(vg,
                            titlePos + vec2(2, titleHeight / 2),
                            titleSize,
                            m_track->name,
                            theme,
                            titleHeight - 2,
                            getContrastFontColorNvg(color),
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

            for (auto g : guis) {
                g->render(vg);
            }
        }
        void dragMoveOn(guibase* target, ivec2 mousepos) override {
            target->trackEntryDragMove(this->m_trackentry, toControlsObjectSpace(mousepos, target));
        }
        void dragReleaseOn(guibase* target, ivec2 mousepos) override {
            target->trackEntryDragRelease(this->m_trackentry, toControlsObjectSpace(mousepos, target));
        }
        void handleRightClick(MouseEvent& evt) override {
            auto trackCtr = dawCtrl->getTrackContainer();
            if (!trackCtr)
                return;
            track_gui_entry_t* entryTrackCtr = nullptr;
            if (!trackCtr->getTrackEntry(m_track, &entryTrackCtr))
                return;
            m_trackentry->parentCtrl->openContextMenu(new guictxtmenu_track(dawCtrl, entryTrackCtr), evt.mousepos);
        }
        guictxtmenu_base* getTooltip(AppCtrl* appctrl) override {
            if (this->m_track->audio) {
                auto tooltip = new guitooltip<guictr_mixertitle>(this);
                return tooltip;
            }
            return nullptr;
        }
    };
    
    guictr_base* createTrackGuiMixerTitle(guictr_mixers* mixer, track_gui_entry_t* entry) {
        return new guictr_mixertitle(mixer, entry);
    }
}// namespace DAW

template<>
void guitooltip<DAW::guictr_mixertitle>::setContent() {
    auto guiPtr = getInstanceOrNull();
    if (!guiPtr) {
        return;
    }
    auto trackEntry = guiPtr->getTrackEntry();
    if (!trackEntry) {
        return;
    }
    auto ptr = trackEntry->track;
    if (!ptr) {
        return;
    }
    using Table::table_entry_t;
    using Table::tbl;
    using Table::tbl_row_t;
    using Table::tblfloat;
    using Table::tblint;
    using Table::tblstr;
    using Table::tblString;
    table.tableWidth = 250;
    {
        table.rows.push_back({ { tblstr{ "track" }, tblString{ ptr->name } } });
        auto audio = ptr->audio;
        table.rows.push_back({ { tblstr{ "stageId" }, tblint{ static_cast<int32_t>(ptr->audio->stageId.stageId) } } });
        table.rows.push_back({ { tblstr{ "inputStageId" }, tblint{ static_cast<int32_t>(ptr->audio->stageId.inputStageId) } } });
        table.rows.push_back({ { tblstr{ "outputStageId" }, tblint{ static_cast<int32_t>(ptr->audio->stageId.outputStageId) } } });
        table.rows.push_back({ { tblstr{ "outputPostStageId" }, tblint{ static_cast<int32_t>(ptr->audio->stageId.outputPostStageId) } } });
        table.rows.push_back({ { tblstr{ "latency input " }, tblint{ audio->getInputLatency() } } });
        table.rows.push_back({ { tblstr{ "latency intern" }, tblint{ audio->getInternalLatencyCustom() } } });
        table.rows.push_back({ { tblstr{ "latency output" }, tblint{ audio->getOutputLatency() } } });
        table.rows.push_back({ { tblstr{ "sampleRate" }, tblint{ audio->sampleFormat.sampleRate } } });
    }
}
