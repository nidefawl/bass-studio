#pragma once
#include <vector>
#include <memory>
#include "config.h"
#include "event.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "exceptions.h"
#include "seq_util.h"
#include "color_util.h"
#include "host/track/track.h"
#include "host/clip/clip.h"
#include "host/daw/clipboard.h"
#include "grid.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/controls/scrollbar.h"
#include "tracktimeline.h"
#include "snapshot/track-snapshot.h"
#include "mouse.h"
#include "keyboard.h"
#include "cursor.h"
#include "platform.h"
#include "dsp_util.h"
#include "host/daw/mainctrl.h"
#include "host/track/trackctr_types.h"
#include "types.h"

void updateStoreLoadSubtracks(guictr_tracks* guiTracks, track_gui_entry_t* entry);
struct track_selection_t {
    int32_t trackIdxMin{};
    int32_t trackIdxMax{};
    bool isContinuous{};
    std::vector<track_t*> tracks;
};
class track_gui_manager_i {
public:
    virtual ~track_gui_manager_i() = default;
    virtual void reset()           = 0;

    virtual bool validTrackIdx(int32_t idx) const       = 0;
    virtual const track_gui_entry_t* at(size_t i) const = 0;
    virtual track_gui_entry_t* atNC(size_t i)           = 0;
    virtual int32_t clampTrackIdx(int32_t idx) const    = 0;

    virtual bool isVisible(const track_gui_entry_t* entry)     = 0;
    virtual const track_gui_vector_td& getTracksVisibleFlat()  = 0;
    virtual const track_gui_vector_td& getTracksTopFlat()      = 0;
    virtual const track_gui_vector_td& getTracksBottomFlat()   = 0;

    virtual bool getTrackEntryCopy(const track_t* t, track_gui_entry_t& out) = 0;
    virtual bool getPointerEntry(const track_t* t, track_gui_entry_t** out)  = 0;
    virtual int32_t getTrackProjectIndex(int32_t guiIdx) const               = 0;

    virtual bool getTrackSelection(const DAW::Cursor& cursor, track_selection_t& sel) const = 0;
};

namespace DAW {
    int32_t getPosYFirstReturnTrack(const track_gui_vector_td& tracksVisibleFlat);
    gui_clip* GetClipFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse);
    gui_clip* GetClipGuiFromTime(track_gui_entry_t* tr, tick_t time);
    editor_view_selection_t GetClipboardViewFromGuiClip(gui_clip* guiClip);
    gui_clip* GetClipGuiFromTimeAndTrackIdx(track_gui_manager_i& iGuiMgr, int32_t trackIdx, tick_t time);
    track_gui_entry_t* getTrackFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse);
    track_gui_entry_t* getTrackFromMouseClosest(track_gui_manager_i& iGuiMgr, ivec2 mouse);
    gui_track_subtrack* getSubTrackFromMouse(track_gui_manager_i& iGuiMgr, ivec2 mouse, bool isDragSnap);
    gui_track* createTrackGui(track_gui_entry_t* _entry, scaled_grid&);// trackcontent.cpp
    gui_clip* createClipGui(guictr_base* parent, track_gui_entry_t* trackentry, clip_t* clip);
    gui_track_controls* createTrackGuiMixer(track_gui_entry_t* _entry, scaled_grid&);// trackcontrols.cpp
}

class guitrack_editor final : public guictr_base {
public:
    track_gui_manager_i& iGuiMgr;
    DAW::Cursor& cursor;
    project_t& project;
    project_globals_t& projectGlobals;
    scaled_grid& grid;
    dragdrop_file_clipboard& dragdrop;
    track_gui_entry_t* trSelected     = nullptr;
    gui_track_subtrack* subTrSelected = nullptr;
    clip_dragaction action;                   // move up in hierachy
    tracklayout_t dragStartLayout;
    int32_t dragStartTick     = 0;
    int32_t dragStartTrackIdx = 0;

    trackstate_t m_resizePreModifyState;
    bool selectionMoved = false;
    std::optional<container_background_image> bgSecondImage;
    guitrack_editor(DawCtrl* const _dawCtrl, track_gui_manager_i& _iGuiMgr, DAW::Cursor& _cursor, project_t& _project, project_globals_t& _projectGlobals, scaled_grid& _grid, dragdrop_file_clipboard& _dragdropclip);
    ~guitrack_editor() override = default;
    scaled_grid& getGrid() { return grid; }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    bool handleKeyInput(KeyEvent& kevt) override;
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);

    void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) override;
    void trackViewDragMove(guitrack_editor* view, MouseEvent& evt) override;
    void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) override;
    void dragSelectionBegin(gui_clip* gClip, MouseEvent& evt);
    void dragSelectionMove(gui_clip* gui, MouseEvent& evt);
    void dragSelectionRelease(gui_clip* gui, MouseEvent& evt);
    void dragClipboardMove(ivec2 local, KeyboardMods kbmods);

    bool clipDropMove(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override;
    bool clipDropFinal(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override;
    void clipDropCancel() override;

    void handleRightClick(MouseEvent& evt) override;

    void renderClip(NVGcontext* vg, const track_gui_entry_t* entry, clip_t* cl, tick_t offset);
    void render(NVGcontext* vg) override;
    void renderDebugPass(NVGcontext* vg);

    void handleDraggedBegin(MouseEvent& evt) override {
        if (evt.type == MouseEventType::M_EVT_DOUBLECLICK) {
            if (evt.guiDragged->trackViewDoubleClick(this, evt)) {
                return;
            }
        }
        evt.guiDragged->trackViewDragBegin(this, evt);
    }
    void handleDraggedMove(MouseEvent& evt) override {
        evt.guiDragged->trackViewDragMove(this, evt);
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        evt.guiDragged->trackViewDragRelease(this, evt);
    }

    void setSelectionRange(clip_t* clicked, track_gui_entry_t* trackClicked) {
        cursor.selRange      = clicked->getLen();
        cursor.selTrackRange = 0;
        cursor.cursorPos     = clicked->time;
        cursor.setTrack(trackClicked->idx);
        cursor.cursorSubTrack   = -1;
        cursor.selSubTrackRange = 0;
    }

    void addSubtrack(track_gui_entry_t* entry, gui_track_subtrack* al, bool insertFront);
    void removeSubtrack(track_gui_entry_t* entry, gui_track_subtrack* al);
    void removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx);
    void removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at);
    void removeAllSubtracks(track_gui_entry_t* entry);
    void addTrackEntry(track_gui_entry_t& e);
    void removeTrackEntry(track_gui_entry_t& e);
    void layout() override;
    guibase* getFocusedContainer() override {
        return this;
    }
};


class guitrack_mixers final : public guictr_base {
    track_gui_manager_i& iGuiMgr;
    project_t& project;

public:
    guitrack_mixers(track_gui_manager_i& _iGuiMgr, project_t& _project)
        : guictr_base(),
          iGuiMgr(_iGuiMgr),
          project(_project) {
        padding      = 0;
        sortChildren = true;
        setCanMouseHit(true);
    }
    bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override;
    void handleRightClick(MouseEvent& evt) override;
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);
    void render(NVGcontext* vg) override;
    void addTrackEntry(track_gui_entry_t& e);
    void removeTrackEntry(track_gui_entry_t& e);
    void layout() override {
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    guibase* getFocusedContainer() override {
        return this;
    }
    bool handleKeyInput(KeyEvent& kevt) override;
};

class te_constants {
protected:
    const uint32_t heightSeperator        = 10;
    const uint32_t heightLoopIndicators   = 24;
    const uint32_t heightTimelineControls = heightLoopIndicators + heightSeperator;
};
class guictr_tracks_loophandles final : public guibase, te_constants {
    //project_t& project;
    project_globals_t& projectGlobals;
    scaled_grid& grid;
    enum dragmode {
        drag_handle_none,
        drag_handle_loopleft,
        drag_handle_loopright,
        drag_handle_loopbar
    };
    dragmode dragHandle = drag_handle_none;

public:
    ivec2 clipViewSize{ 0, 0 };
    guictr_tracks_loophandles(project_t&, project_globals_t& _projectGlobals, scaled_grid& _grid)
        : guibase(),
          /*project(_project),*/
          projectGlobals(_projectGlobals),
          grid(_grid) {
    }
    int32_t dragOffset = 0;
    void handleDraggedBegin(MouseEvent& evt) override {
        dragHandle  = drag_handle_none;
        ivec2 local = evt.relMousepos;
        dragHandle  = getDragZone(local);
        dragOffset  = local.x - (int32_t) grid.tickToScreenD(projectGlobals.loopStart);
    }
    void handleDraggedMove(MouseEvent& evt) override {
        if (dragHandle == drag_handle_none) {
            return;
        }
        int32_t mousePosX = evt.relMousepos.x;
        if (dragHandle == drag_handle_loopbar) {
            mousePosX -= dragOffset;
        }
        tick_t tickAt     = grid.screenToTickSnap(mousePosX, isAlt(evt.kbmods) ? SNAP_OFF : SNAP_ON);
        tick_t curLoopEnd = projectGlobals.loopStart + projectGlobals.loopLen;

        if (dragHandle == drag_handle_loopright) {
            tick_t tickDelta = (tickAt - curLoopEnd);
            tick_t newLen    = projectGlobals.loopLen + tickDelta;
            if (newLen > 0) {
                projectGlobals.loopLen = newLen;
            }
        }
        if (dragHandle == drag_handle_loopleft) {
            tick_t curLoopStart = projectGlobals.loopStart;
            tick_t tickDelta    = (tickAt - curLoopStart);
            tick_t newStart     = projectGlobals.loopStart + tickDelta;
            if (newStart < curLoopEnd) {
                projectGlobals.loopStart = newStart;
                projectGlobals.loopLen   = curLoopEnd - newStart;
            }
        }
        if (dragHandle == drag_handle_loopbar) {
            tick_t curLoopStart = projectGlobals.loopStart;
            tick_t tickDelta    = (tickAt - curLoopStart);
            projectGlobals.loopStart += tickDelta;
        }
    }
    void handleDraggedRelease(MouseEvent& evt) override {
        dragHandle = drag_handle_none;
    }
    static float dist(float x, float y, ivec2 mpos) {
        x = x - mpos.x;
        y = y - mpos.y;
        return x * x + y * y;
    }
    dragmode getDragZone(ivec2 local) {
        struct dist_draghandle {
            float dist    = 0;
            dragmode mode = drag_handle_none;
        };
        float dragTop = heightLoopIndicators / 2.0f;
        float distBar = std::numeric_limits<float>::max();
        float barSX   = clipLoopStartScrX();
        float barEX   = clipLoopEndScrX();
        if (local.x >= barSX && local.x < barEX && local.y >= 0 && local.y < (int) heightLoopIndicators) {
            distBar = DRAG_RANGE * DRAG_RANGE * 0.8f;
        }
        std::vector<dist_draghandle> hndls{
            { dist(barSX, dragTop, local), dragmode::drag_handle_loopleft },
            { dist(barEX, dragTop, local), dragmode::drag_handle_loopright },
            { distBar, dragmode::drag_handle_loopbar }
        };
        std::sort(hndls.begin(), hndls.end(), [](dist_draghandle const& a, dist_draghandle const& b) {
            return a.dist < b.dist;
        });
        if (hndls[0].dist < DRAG_RANGE * DRAG_RANGE) {
            return hndls[0].mode;
        }

        return drag_handle_none;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos)) {
            ivec2 local = this->toContainerSpace(mpos);
            if (evt.type <= MouseHitType::MOUSE_RIGHT) {
                dragmode mode = getDragZone(local);
                if (mode == dragmode::drag_handle_loopleft) {
                    evt.requestCursor(CURSOR_CLIP_SIZE_LEFT);
                    evt.requestFocus(this);
                    return true;
                }
                if (mode == dragmode::drag_handle_loopright) {
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
    float clipLoopStartScrX() {
        return (float) grid.tickToScreenD(projectGlobals.loopStart);
    }
    float clipLoopEndScrX() {
        return (float) grid.tickToScreenD(projectGlobals.loopStart + projectGlobals.loopLen);
    }
    void render(NVGcontext* vg) override {
        ivec2 cs = clipViewSize;
        if (cs.x <= 0 || cs.y <= 0)
            return;
        nvgIntersectScissor(vg, pos.x, pos.y, cs.x, cs.y);
        nvgTranslate(vg, pos.x, pos.y);
        nvgBeginPath(vg);
        nvgRect(vg, -2, 0, cs.x + 2, size.y);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_DRK));
        nvgFill(vg);

        for (grid_div g : grid.gridList) {
            nvgBeginPath(vg);
            nvgMoveTo(vg, g.screenpos, 0);
            nvgLineTo(vg, g.screenpos, heightLoopIndicators);
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
        nvgRect(vg, -2, heightLoopIndicators, cs.x + 2, heightSeperator);
        nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER2));
        nvgFill(vg);


        const NVGcolor colLI        = theme->getColor(GuiColor::COL_LOOPHANDLES);
        const NVGcolor colLIStroke  = theme->getFrameColorOutline();
        const float strokeWidthLI   = 1.0f;
        const float wLoopInidicator = heightLoopIndicators;


        int yOffset      = 0;
        float tickBeginX = clipLoopStartScrX();
        float tickEndX   = clipLoopEndScrX();
        if (tickBeginX > tickEndX) {
            std::swap(tickBeginX, tickEndX);
        }
        if (!(tickBeginX - wLoopInidicator > cs.x || tickEndX + wLoopInidicator < 0)) {
            float barBeginX = math::max(-wLoopInidicator, tickBeginX);
            float barEndX   = math::min(cs.x + wLoopInidicator, tickEndX);
            nvgBeginPath(vg);
            nvgRect(vg, barBeginX, yOffset, barEndX - barBeginX, heightLoopIndicators);

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
                nvgLineTo(vg, tickEndX, cs.y - yOffset + 1);
                nvgStrokeColor(vg, colLI);
                nvgStrokeWidth(vg, strokeWidthLI);
                nvgStroke(vg);
                drawTri(vg, tickEndX, yOffset, wLoopInidicator, 1, colLI, colLIStroke, strokeWidthLI);
            }
        }
        float xJmpFrom = grid.tickToScreenD(dawCtrl->getDaw()->tickJmpFrom);
        float xJmpTo   = grid.tickToScreenD(dawCtrl->getDaw()->tickJmpTo);
        nvgBeginPath(vg);
        nvgMoveTo(vg, xJmpFrom, yOffset);
        nvgLineTo(vg, xJmpFrom, cs.y - yOffset + 1);
        nvgStrokeColor(vg, G_YELLOW_DRK);
        nvgStrokeWidth(vg, strokeWidthLI);
        nvgStroke(vg);
        nvgBeginPath(vg);
        nvgMoveTo(vg, xJmpTo, yOffset);
        nvgLineTo(vg, xJmpTo, cs.y - yOffset + 1);
        nvgStrokeColor(vg, G_GREEN_DRK);
        nvgStrokeWidth(vg, strokeWidthLI);
        nvgStroke(vg);

        yOffset += heightLoopIndicators;
    }
};
class track_gui_manager_t final : public track_gui_manager_i {
    friend class guictr_tracks;
    track_gui_vector_td entries;
    track_gui_vector_td trackEntriesTop;
    track_gui_vector_td trackEntriesBottom;
    track_gui_vector_td tracksVisibleFlat;

public:
    bool getTrackEntryCopy(const track_t* t, track_gui_entry_t& out) override;
    bool getPointerEntry(const track_t* t, track_gui_entry_t** out) override;
    bool getTrackEntry(const track_t* t, track_gui_entry_t** out);

    void removeTrack(track_gui_entry_t& entry) {
        auto it = std::remove_if(begin(entries), end(entries), [&entry](track_gui_entry_t* e) {
            return e->track == entry.track;
        });
        if (it == entries.end()) {
            dbgassert(0);
            return;
        }
        removeEntry(trackEntriesTop, &entry);
        removeEntry(trackEntriesBottom, &entry);
        removeEntry(tracksVisibleFlat, &entry);
        delete &entry;
        entries.erase(it, entries.end());
    }
    void addTrack(track_gui_entry_t* entry) {
#ifndef NDEBUG
        auto it = std::find_if(begin(entries), end(entries), [entry](track_gui_entry_t* e) {
            if (e->track == entry->track) {
                return true;
            }
            return false;
        });
        dbgassert(it == entries.end() && "Attempt to add track_gui_entry_t twice");
#endif
        entries.push_back(entry);
    }
    /**
     * checks if entry is visible.
     * A track_gui_entry_t is visible if
     * none of its parents have the field
     * this->layout.hideTrack == true
     */
    bool isVisible(const track_gui_entry_t* entry) override {
        bool bHidden = false;
        track_t* p   = entry->track->parent;
        while (!bHidden && p) {
            track_gui_entry_t* parentEntry{};
            if (!getPointerEntry(p, &parentEntry)) {
                return false;
            }
            bHidden |= parentEntry->isHidden();
            p = p->parent;
        }
        return !bHidden;
    }
    bool validTrackIdx(int32_t idx) const override {
        return idx >= 0 && idx < (int32_t) tracksVisibleFlat.size();
    }
    const track_gui_entry_t* at(const size_t i) const override {
        if (!validTrackIdx(i)) {
            return nullptr;
        }
        return tracksVisibleFlat.at(i);
    }
    track_gui_entry_t* atNC(const size_t i) override {
        if (!validTrackIdx(i)) {
            return nullptr;
        }
        return tracksVisibleFlat.at(i);
    }
    int32_t clampTrackIdx(int32_t idx) const override {
        return math::max(0, math::min((int32_t) tracksVisibleFlat.size() - 1, idx));
    }
    void updateVisibleTracks(trackallcontainer_t& trackList) {
        /** turn tree structure into linear pointer array with trackTop at the beginning and the deepest child at the end **/
        track_gui_vector_td vecNewTracksFlat;
        std::deque<track_t*> stack;
        stack.insert(stack.begin(), trackList.cbeginTree(), trackList.cendTree());
        trackEntriesTop.clear();
        trackEntriesBottom.clear();
        while (!stack.empty()) {
            track_t* current = stack.front();
            stack.pop_front();
            track_gui_entry_t* entry{};

            if ((getPointerEntry(current, &entry))) {
                if (!entry->isHidden() && !current->children.empty()) {
                    stack.insert(stack.begin(), current->children.cbegin(), current->children.cend());
                }
                dbgassert(isVisible(entry));
                dbgassert(entry->track == current);
                if (TRACKTYPE_TO_CTR(entry->track->type) == TRACK_CTR_MIDIAUDIO) {
                    trackEntriesTop.push_back(entry);
                } else {
                    trackEntriesBottom.push_back(entry);
                }
                entry->idx = CtrSize(vecNewTracksFlat);
                vecNewTracksFlat.push_back(entry);
            }
        }

        tracksVisibleFlat = vecNewTracksFlat;
    }
    const track_gui_vector_td& getTracksVisibleFlat() override {
        return tracksVisibleFlat;
    }
    const track_gui_vector_td& getTracksTopFlat() override {
        return trackEntriesTop;
    }
    const track_gui_vector_td& getTracksBottomFlat() override {
        return trackEntriesBottom;
    }
    void reset() override {
        dbgassert(entries.empty());
        tracksVisibleFlat.clear();
        trackEntriesTop.clear();
        trackEntriesBottom.clear();
    }
    int32_t getTrackProjectIndex(int32_t guiIdx) const override {
        if (assert_expr(validTrackIdx(guiIdx))) {
            return at(guiIdx)->track->projectIdx;
        }
        return -1;
    }
    bool getTrackSelection(const DAW::Cursor& cursor, track_selection_t& sel) const override {
        track_selection_t t;
        t.isContinuous = true;
        t.trackIdxMin  = 0;
        t.trackIdxMax  = CtrSize(tracksVisibleFlat);
        if (validTrackIdx(cursor.getTrackBegin())) {
            t.trackIdxMin = at(cursor.getTrackBegin())->track->projectIdx;
        }
        if (validTrackIdx(cursor.getTrackEnd())) {
            t.trackIdxMax = at(cursor.getTrackEnd())->track->projectIdx;
        }
        if (t.trackIdxMax - t.trackIdxMin >= 0) {
            t.tracks.reserve(math::max(1, t.trackIdxMax - t.trackIdxMin + 1));
            for (int32_t i = math::max<int32_t>(0, cursor.getTrackBegin()); i <= cursor.getTrackEnd() && i < CtrSize(tracksVisibleFlat); i++) {
                const track_gui_entry_t* const entry = tracksVisibleFlat[i];
                t.tracks.push_back(entry->track);
            }
        }
        sel = t;
        return true;
    }
};
class guitrack_topleft final : public guictr_base {
    guictr_tracks& ctrTracks;
    track_gui_manager_i& iGuiMgr;
    project_t& project;
    guibuttontoggle btnFoldAll;
    guibuttontoggle btnCopyAutomation;
    bool isFolded = false;
    std::vector<guibuttontoggle*> guiButtons;

public:
    guitrack_topleft(guictr_tracks& _ctrTracks, DawCtrl* const _dawCtrl, track_gui_manager_i& _iGuiMgr, project_t& _project);
    ~guitrack_topleft() override {
        remove(&btnCopyAutomation);
        remove(&btnFoldAll);
    }
    void buttonClicked(guibase* _button) override;
    void layout() override {
        const int32_t CONST_PADDING_TRACK_CONTROLS = theme->get(GuiConstant::CONST_PADDING_TRACK_CONTROLS);
        const int32_t TRACK_HEIGHT_STEP   = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);

        int32_t inset  = CONST_PADDING_TRACK_CONTROLS;
        int32_t i2     = inset * 2;
        int32_t h      = TRACK_HEIGHT_STEP - i2;
        int buttonSize = h;
        ivec2 btnPos   = { inset, inset };
        for (auto btn : guiButtons) {
            btn->size = { buttonSize, buttonSize };
            btn->setRadius(h / 3.f);
            btn->pos = btnPos;
            btnPos.x += buttonSize;
        }
        for (auto gui : guis) {
            gui->layout();
        }
    }
};
class guictr_tracks final : public guictr_base, grid_changed_cb, te_constants, public gui_scrollcontainer {
    friend class guitrack_editor;
    int32_t trackContainerGlobalIndex = 0;

public:
    scaled_grid m_grid;
    project_t& project;
    project_globals_t& projectGlobals;
    track_gui_manager_t guiMgr;
    guitrack_topleft trackTopLeft;
    guitrack_mixers trackControls;
    guitrack_editor trackView;
    guitrack_timeline trackTimeline;
    guictr_tracks_loophandles loophandles;

protected:
    gui_scrollbar scrollbar;
    int32_t contentHeight   = 0;
    int32_t contentViewSize = 0;

public:
    guictr_tracks(DawCtrl* _dawCtrl, DAW::Cursor& _cursor, DAW::TrackSelection& _trackSelection, project_t& _project, project_globals_t& _projectGlobals, dragdrop_file_clipboard& _dragdropclip);
    ~guictr_tracks() override;
    scaled_grid& getGrid() {
        return m_grid;
    }
    int32_t setTrackPosition(track_gui_entry_t* e, int32_t y, bool isBottom);
    int32_t getTrackTotalHeight(track_gui_entry_t* e);
    bool mouseHitTest(ivec2 v, MouseHitEvt& evt) override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void updateVisibleTracks();
    void layoutVisibleTracks();

    void onChildLayoutChanged(guibase* g) override;
    void gridChanged(scaled_grid& _grid) override {
        dawCtrl->updateVisibleTrackContents();
    }
    bool handleEditorCommand(DAW::UI::CommandContext& ctxt);
    ivec2 getScrollTotalSize() const override {
        ivec2 cs = getSizeContent();
        cs.y     = contentHeight;
        return cs;
    }
    ivec2 getScrollViewSize() const override {
        ivec2 cs = getSizeContent();
        cs.y     = contentViewSize;
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
    void showAutomationLane(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx);
    void addSubTrack(track_gui_entry_t* entry, gui_track_subtrack* subtrack, bool insertFront);
    void removeSubtrack(track_gui_entry_t* entry, gui_track_subtrack* subtrack);

    gui_track_automationlane* addAutomationLane(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx, bool insertFront);
    void removeAutomationLane(gui_track_automationlane* al);
    void removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at, int32_t paramIdx);
    void removeAllAutomationLanes(track_gui_entry_t* entry, automatable_t* at);
    void removeAllSubtracks(track_gui_entry_t* entry);
    void loadTrackLayouts(trackcontainer_snapshot_t& in);
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


    void trackEntryDragMove(gui_track* g, ivec2 mousepos) override;
    void trackEntryDragRelease(gui_track* g, ivec2 mousepos) override;

    void pluginEntryDragMove(gui_pluginlist_entry* g, ivec2 mousepos) override;
    void pluginEntryDragRelease(gui_pluginlist_entry* g, ivec2 mousepos) override;
    void pluginMultiDragMove(guictr_dragged_plugins* g, ivec2 mousepos) override;
    void pluginMultiDragRelease(guictr_dragged_plugins* g, ivec2 mousepos) override;

    bool clipDropMove(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override;
    bool clipDropFinal(dragdrop_file_clipboard& clip, ivec2 mousepos, KeyboardMods kbmods) override;
};