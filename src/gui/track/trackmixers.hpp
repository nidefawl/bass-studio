#pragma once
#include "renderresources.hpp"
#include "trackctr.hpp"

namespace DAW {
    class guictr_mixers_mixer;
};

class guictr_mixers final : public guictr_base, public gui_scrollcontainer {
    friend class guitrack_editor;
    friend class DAW::guictr_mixers_mixer;
    friend class guictr_mixers_options;

    class guictr_mixers_options final : public guictr_base {
        guictr_mixers* const m_parent;
        std::array<guibuttontoggle, 5> btnViews;
    public:
        explicit guictr_mixers_options(guictr_mixers* _parent) : m_parent(_parent) {
            setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
            padding = 2;
            for (auto& btn : btnViews) {
                btn.id = static_cast<int32_t>(&btn - btnViews.data());
                btn.drawFn   = drawTextureSymbol;
                String text;
                switch (btn.id) {
                    case 0:
                        text = "Layout Wide/Compact";
                        btn.setStateRef(&m_parent->bWideLayout);
                        btn.getIcon = [p = m_parent] { return p->bWideLayout ? ICON_ARR_RIGHT : ICON_ARR_DOWN; };
                        break;
                    case 1:
                        text = "Show Sends";
                        btn.icon = ICON_MODULATION;
                        btn.setStateRef(&m_parent->bShowSends);
                        btn.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
                        break;
                    case 2:
                        text = "Show Inputs/Outputs";
                        btn.icon = ICON_MIDIPLUG;
                        btn.setStateRef(&m_parent->bShowIO);
                        btn.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
                        break;
                    case 3:
                        text = "Show Return Tracks";
                        btn.icon = ICON_MODULATION_INPUT;
                        btn.setStateRef(&m_parent->bShowReturnTracks);
                        btn.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
                        break;
                    case 4:
                        text = "Show Master Tracks";
                        btn.icon = ICON_ADJUST;
                        btn.setStateRef(&m_parent->bShowMasterTracks);
                        btn.colorActive = GuiColor::COL_BTN_BG_SHOW_ACTIVE;
                        break;
                    default:
                        break;
                }
                btn.setLabel(text);
                btn.setTooltipText(text);
                btn.setButtonColor(GuiColor::COL_BTN_BG_SHOW_ACTIVE);
                add(&btn);
            }
        }
        ~guictr_mixers_options() override {
            removeGuis();
        }
        void buttonClicked(guibase* button) override {
            for (auto& btn : btnViews) {
                if (&btn == button) {
                    switch (btn.id) {
                        case 0: {
                            m_parent->bWideLayout = !m_parent->bWideLayout;
                            {
                                track_gui_vector_td& tracks = m_parent->guiMgr.tracksVisibleFlat;
                                for (track_gui_entry_t* entry : tracks) {
                                    switch (TRACKTYPE_TO_CTR(entry->track->type)) {
                                        case TRACK_CTR_MIDIAUDIO:
                                        default:
                                            entry->layout.height = m_parent->bWideLayout ? 5 : 3;
                                            break;
                                        case TRACK_CTR_RETURN:
                                            entry->layout.height = m_parent->bWideLayout ? 6 : 3;
                                            break;
                                        case TRACK_CTR_MASTER:
                                            entry->layout.height = m_parent->bWideLayout ? 8 : 6;
                                            break;
                                    }
                                }
                            }
                            break;
                        }
                        case 1: {
                            m_parent->bShowSends = !m_parent->bShowSends;
                            break;
                        }
                        case 2: {
                            m_parent->bShowIO = !m_parent->bShowIO;
                            break;
                        }
                        case 3: {
                            m_parent->bShowReturnTracks = !m_parent->bShowReturnTracks;
                            for (track_gui_entry_t* entry : m_parent->guiMgr.tracksVisibleFlat) {
                                if (entry->track->type == TRACK_TYPE_RETURN) {
                                    // entry->layout.hideTrack = !m_parent->bShowMasterTracks;
                                }
                            }
                            break;
                        }
                        case 4: {
                            m_parent->bShowMasterTracks = !m_parent->bShowMasterTracks;
                            for (track_gui_entry_t* entry : m_parent->guiMgr.tracksVisibleFlat) {
                                if (entry->track->type == TRACK_TYPE_MASTER) {
                                    // entry->layout.hideTrack = !m_parent->bShowMasterTracks;
                                }
                            }
                            break;
                        }
                        default:
                            break;
                    }
                    m_parent->updateVisibleTracks();
                    m_parent->layout();
                    break;
                }
            }
            if (parent) {
                parent->buttonClicked(this);
            }
        }
    };
    class guictr_mixers_content : public guictr_base {
        track_gui_manager_t& guiMgr;
    public:
        guictr_mixers_content(track_gui_manager_t& _guiMgr) 
            : guictr_base(),
            guiMgr(_guiMgr)
        {
            padding = 0;
            margin  = 0;
            setCanMouseHit(false);
            setBackgroundRendered(false);
            sortChildren = true;
        }
        void addTrackEntry(track_gui_entry_t& e);
        
        void removeTrackEntry(track_gui_entry_t& e);

        void render(NVGcontext* vg) override;
    };
protected:
    int32_t trackMixerGlobalIndex = 0;
    int32_t contentWidth    = 0;
    int32_t contentViewSize = 0;
    bool bWideLayout = false;
    bool bShowSends = true;
    bool bShowIO = false;
    bool bShowReturnTracks = true;
    bool bShowMasterTracks = true;
public:
    project_t& project;
    project_globals_t& projectGlobals;
    track_gui_manager_t guiMgr;
    gui_scrollbar scrollbar;
    guictr_mixers_content trackMixers;
    guictr_mixers_options mixerOptions;
public:
    guictr_mixers(DawCtrl* _dawCtrl, DAW::Cursor& _cursor, DAW::TrackSelection& _trackSelection, project_t& _project, project_globals_t& _projectGlobals, dragdrop_file& _dragdropclip);
    ~guictr_mixers() override;
    int32_t setTrackPosition(track_gui_entry_t* e, int32_t x, bool isBottom);
    int32_t getTrackTotalWidth(track_gui_entry_t* e);
    bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void updateVisibleTracks();

    void onChildLayoutChanged(guibase* g) override;
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);
    ivec2 getScrollTotalSize() const override {
        ivec2 cs = getSizeContent();
        cs.x     = contentWidth;
        return cs;
    }
    ivec2 getScrollViewSize() const override {
        ivec2 cs = getSizeContent();
        cs.x     = contentViewSize;
        return cs;
    }
    void scrollOffsetChanged(int dir, float offset) override;
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;
    void setScrollOffset(float offset) {
        this->scrollbar.setScrollOffset(offset);
    }
    void scrollToPixelOffset(double pixelOffset) {
        this->scrollbar.scrollTo(pixelOffset);
    }
    void scrollTo(guibase* g);
    float getScrollOffset() const {
        return this->scrollbar.scrollOffset;
    }
    double getScrollOffsetPixels() const {
        return this->scrollbar.toPixels();
    }
    void onRemove() override;
    void onAdded() override;
    void removeTrack(track_t* track, int flags);
    void addTrack(track_t* track, int flags);
    void removeAllTracks();
    void addAllTracks();
    void loadMixerLayouts(trackcontainer_snapshot_t& in);
   
    bool getTrackEntry(const track_t* t, track_gui_entry_t** out) {
        return guiMgr.getTrackEntry(t, out);
    }
    bool getPointerEntry(track_t* t, track_gui_entry_t** out) {
        return guiMgr.getPointerEntry(t, out);
    }
    bool isTrackEntryVisible(const track_gui_entry_t* entry) {
        return guiMgr.isVisible(entry);
    }
    void resetView();
    void handleRightClick(MouseEvent& evt) override;
    void trackEntryDragMove(track_gui_entry_t* trackEntry, ivec2 mousepos) override;
    void trackEntryDragRelease(track_gui_entry_t* trackEntry, ivec2 mousepos) override;

    void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override;
    void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override;
    void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override;
    void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override;
};
