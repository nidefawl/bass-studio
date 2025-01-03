#pragma once
#include "renderresources.h"
#include "trackctr.h"

namespace DAW {
    class guictr_mixers_mixer;
};

class guictr_mixers final : public guictr_base, public gui_scrollcontainer {
    friend class guitrack_editor;
    friend class DAW::guictr_mixers_mixer;
    friend class guictr_mixers_options;
    int32_t trackMixerGlobalIndex = 0;

    class guictr_mixers_options final : public guictr_base {
        guictr_mixers* const m_parent;
        class guibutton_ctr_mixers_options : public guibutton {
        public:
            bool bIsSelected = false;
            int32_t btnIndex = 0;
            int32_t getIndex() const {
                return btnIndex;
            }
            guibutton_ctr_mixers_options() = default;
            bool getState() const override {
                return bIsSelected;
            }
            void setState(bool b) {
                bIsSelected = b;
            }
        };
        std::array<guibutton_ctr_mixers_options, 3> btnViews;
    public:
        explicit guictr_mixers_options(guictr_mixers* _parent) : m_parent(_parent) {
            setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
            padding = 6;
            margin  = 6;
            snapSides.y = snapSides.w = 0;
            snapSides.x = snapSides.z = 1;
            for (auto& btn : btnViews) {
                btn.btnIndex = static_cast<int32_t>(&btn - btnViews.data());
                btn.drawFn   = drawTextureSymbol;
                String text;
                switch (btn.btnIndex) {
                    case 0:
                        text = "Layout Wide/Compact";
                        btn.drawParm = ICON_ARR_LEFT;
                        btn.setState(m_parent->bWideLayout);
                        break;
                    case 1:
                        text = "Show Sends";
                        btn.drawParm = ICON_MODULATION;
                        btn.setState(m_parent->bShowSends);
                        break;
                    case 2:
                        text = "Show Inputs/Outputs";
                        btn.drawParm = ICON_MIDIPLUG;
                        btn.setState(m_parent->bShowIO);
                        break;
                    default:
                        break;
                }
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
                    btn.setState(!btn.getState());
                    switch (btn.getIndex()) {
                        case 2:
                            m_parent->bShowIO = btn.getState();
                            btnViews[0].setState(true);
                            break;
                        case 0: {
                            break;
                        }
                        case 1:
                            m_parent->bShowSends = btn.getState();
                            break;
                        default:
                            break;
                    }
                    m_parent->bWideLayout = btnViews[0].getState();
                    btnViews[0].drawParm = m_parent->bWideLayout ? ICON_ARR_RIGHT : ICON_ARR_LEFT;
                    track_gui_vector_td& tracks = m_parent->guiMgr.tracksVisibleFlat;
                    for (track_gui_entry_t* entry : tracks) {
                        entry->layout.height = m_parent->bWideLayout ? 6 : 4;
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
        public:
        guictr_mixers_content() : guictr_base() {
            padding = 0;
            margin  = 0;
            setCanMouseHit(false);
            setBackgroundRendered(false);
            sortChildren = true;
        }
        void addTrackEntry(track_gui_entry_t& e);
        
        void removeTrackEntry(track_gui_entry_t& e);
    };
protected:
    int32_t contentWidth    = 0;
    int32_t contentViewSize = 0;
    bool bWideLayout = false;
    bool bShowSends = true;
    bool bShowIO = false;
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
   
    bool getTrackEntry(track_t* t, track_gui_entry_t** out) {
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
