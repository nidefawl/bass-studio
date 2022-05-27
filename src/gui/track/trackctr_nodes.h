#pragma once
#include <vector>
#include <memory>
#include "config.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "exceptions.h"
#include "seq_util.h"
#include "color_util.h"
#include "track.h"
#include "clip.h"
#include "clipboard.h"
#include "grid.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/controls/scrollbar.h"
#include "tracktimeline.h"
#include "mouse.h"
#include "keyboard.h"
#include "cursor.h"
#include "platform.h"
#include "dsp_util.h"
#include "host/mainctrl.h"
#include "trackctr.h"

enum class GraphType {
    Top,
    Bottom,
};

class gui_graph_entry : public guictr_base {
    friend class gui_graph;
    friend class guictr_nodes_editor;

protected:
    int icon        = 0;
    bool selected   = false;

public:
    gui_graph_entry() : guictr_base() {
        setCanMouseHit(true);
    }
    ~gui_graph_entry() override = default;
    bool contains(ivec2 mpos) const override {
        if (mpos.x >= pos.x &&
            mpos.y >= pos.y &&
            mpos.x < pos.x + size.x &&
            mpos.y < pos.y + size.y)
            return true;
        ivec2 localPos = toContainerSpace(mpos);
        for (auto* gui : guis) {
            if (gui->isVisible() && gui->contains(localPos))
                return true;
        }
        return false;
    }
    void render(NVGcontext* vg) override;
    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void dragMoveOn(guibase* target, ivec2 mousepos) override    = 0;
    void dragReleaseOn(guibase* target, ivec2 mousepos) override = 0;
    virtual String getText()                                     = 0;
    bool isDragMoveable() override {
        return true;
    }
    bool setScissorTransformContainer(NVGcontext* vg) override;
};
class gui_graph_n;
class gui_graph : public guictr_base {
public:
    class guictr_graph_impl;
private:
    guictr_graph_impl* const impl;
protected:
    int32_t first       = 0;
    int32_t last        = 0;
    int rowHeight       = 30;
    ivec4 rowMargin     = { 0, 0, 0, 0 };
    bool renderHR       = false;
    int32_t selectedIdx = -1;
    float scale         = 1.0f;
    vec2 offset{ 0 };
    vec2 prevOffset{ 0 };

public:
    GraphType graphType = GraphType::Top;
    gui_graph();
    ~gui_graph() override;
    ivec2 toParentSpace(ivec2 localCoord) const override;
    ivec2 toContainerSpace(ivec2 in) const override;
    vec2 toParentSpace2f(vec2 localCoord) const override;
    vec2 toContainerSpace2f(vec2 in) const override;
    ivec2 toScreenSpace(ivec2 in) const override;
    void onTick(AppCtrl* appctrl) override;
    int32_t getSelectedIdx() {
        return selectedIdx;
    }
    void setSelectedIdx(int32_t idx) {
        this->selectedIdx = idx;
    }
    void setRowMargin(ivec4 _rowMargin) {
        rowMargin = _rowMargin;
    }
    void setRenderHR(bool _renderHR) {
        renderHR = _renderHR;
    }
    void setRowHeight(int h) {
        rowHeight = h;
    }
    void updateList(bool resetPositions);

    void render(NVGcontext* vg) override;

    void setList(std::vector<gui_graph_entry*> _newList);
    void determineSize(ivec2& prefSize) override /* const */ {
        int x = 0;
        int y = 0;
        for (guibase* gui : guis) {
            x = math::max(x, gui->right());
            y = math::max(y, gui->bottom());
        }
        prefSize.x = math::max(prefSize.x, x);
        prefSize.y = math::max(prefSize.y, y);
    }
    void layout() override {
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    void reset();
    void refresh();
    void buttonClicked(guibase* _button) override {
        if (parent) parent->buttonClicked(_button);
    }
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;

    void handleDraggedBegin(MouseEvent& evt) override;
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override;
    void handleRightClick(MouseEvent& evt) override;
};

class guictr_nodes_editor : public guictr_base, te_constants, public gui_scrollcontainer {
    class guictr_nodes_editor_impl;
    guictr_nodes_editor_impl* const impl;

    friend class guitrack_editor;

public:
    project_t& project;
    gui_graph graph;

protected:
    gui_scrollbar scrollbar;

public:
    guictr_nodes_editor(DAW::Cursor& _cursor, project_t& _project, dragdrop_midifile& _dragdropclip);
    ~guictr_nodes_editor() override;
    void render(NVGcontext* vg) override;
    void scrollTo(guibase* g);
    void layout() override;

    void onChildLayoutChanged(guibase* g) override {
        layout();
    }

    ivec2 getScrollTotalSize() const override {
        ivec2 cs = getSizeContent();
        return cs;
    }
    ivec2 getScrollViewSize() const override {
        return graph.size;
    }
    void scrollOffsetChanged(int dir, float offset) override;
    void setScrollOffset(float offset) {
        this->scrollbar.setScrollOffset(offset);
    }
    float getScrollOffset() const {
        return this->scrollbar.scrollOffset;
    }
    void buttonClicked(guibase* _button) override {
        if (parent) parent->buttonClicked(_button);
    }
    void reset();
    void refresh();
    void resetPositions();
    void resetRouting();
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    bool handleKeyInput(KeyEvent& event) override;
    void onTick(AppCtrl* appctrl) override;
    guibase* getFocusedContainer() override {
        return this;
    }
    bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override {
        return false;
    }
    void handleRightClick(MouseEvent& evt) override;
};

class guictr_nodes_splitview : public guictr_base, public splitter_cb {
public:
    project_t& project;

private:
    guictr_nodes_editor graphTop;
    guictr_nodes_editor graphBottom;
    Splitter splitter;

public:
    guictr_nodes_splitview(DAW::Cursor& _cursor, project_t& _project, dragdrop_midifile& _dragdropclip);
    ~guictr_nodes_splitview() override;
    void layout() override;
    void onChildLayoutChanged(guibase* g) override;
    void reset();
    void refresh();
    void buttonClicked(guibase* _button) override;
    void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) override;
    ivec2 getContainerSize() override;
    void onPluginSelected();
};
