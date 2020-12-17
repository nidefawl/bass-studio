#pragma once
#include <stdbool.h>
#include <stdint.h>
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
#include "gui.h"
#include "guicontainer.h"
#include "scrollbar.h"
#include "tracktimeline.h"
#include "mouse.h"
#include "keyboard.h"
#include "cursor.h"
#include "platform.h"
#include "dsp_util.h"
#include "../host/mainctrl.h"
#include "trackctr.h"

class gui_graph_entry : public guictr_base {
	friend class gui_graph;
	friend class guictr_nodes_editor;
protected:
	int icon = 0;
	bool selected = false;
	float rowHeight = 0;
public:
	gui_graph_entry() : guictr_base() {
		setCanMouseHit(true);
	}
	virtual ~gui_graph_entry() {
	}
	virtual void render(NVGcontext* vg);
	virtual void handleDraggedBegin(MouseEvent& evt);
	virtual void handleDraggedMove(MouseEvent& evt);
	virtual void handleDraggedRelease(MouseEvent& evt);
	virtual void dragMoveOn(guibase* target, ivec2 mousepos) = 0;
	virtual void dragReleaseOn(guibase* target, ivec2 mousepos) = 0;
	virtual String getText() = 0;
	bool isDragMoveable() {
		return true;
	}
};
class gui_graph_n;
class gui_graph : public guictr_base {
	class guictr_graph_impl;
	guictr_graph_impl * const impl;
protected:
	int32_t first = 0;
	int32_t last = 0;
	int rowHeight = 30;
	ivec4 rowMargin = {0, 0, 0, 0};
	bool renderHR = false;
	int32_t selectedIdx = -1;
	float scale = 1.0f;
	vec2 offset{0};
	vec2 prevOffset{0};
public:
	bool isTrackGraph = false;
	gui_graph();
	~gui_graph();
	virtual ivec2 toParentSpace(ivec2 localCoord);
	virtual ivec2 toContainerSpace(ivec2 in);
	vec2 toContainerSpace2f(vec2 in);
	virtual ivec2 toScreenSpace(ivec2 in) const;
	void onTick(AppCtrl* appctrl) override;
	int32_t getSelectedIdx() {
		return selectedIdx;
	}
	void setSelectedIdx(int32_t selectedIdx) {
		this->selectedIdx = selectedIdx;
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
	virtual void determineSize(ivec2& prefSize)/* const */ {
		int x = 0; int y = 0;
		for (guibase* gui : guis) {
			x = math::max(x, gui->right());
			y = math::max(y, gui->bottom());
//			gui->pos = ivec2(x+rowMargin.x, y+rowMargin.y);
//			gui->size = ivec2(entryW - (rowMargin.x+rowMargin.z), rowHeight - (rowMargin.y+rowMargin.w));
//
//			y += rowHeight;
		}
		prefSize.x = math::max(prefSize.x, x);
		prefSize.y = math::max(prefSize.y, y);
	}
	void layout() {
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	void reset();
	void refresh();
	void buttonClicked(guibase* _button) {
		if (parent) parent->buttonClicked(_button);
	}
	bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override;

	void handleDraggedBegin(MouseEvent& evt) override;
	void handleDraggedMove(MouseEvent& evt) override;
	void handleDraggedRelease(MouseEvent& evt) override;
};

class guictr_nodes_editor : public guictr_base, te_constants, public gui_scrollcontainer {
	class guictr_nodes_editor_impl;
	guictr_nodes_editor_impl * const impl;

	friend class guitrack_editor;
public:
	project_t& project;
	gui_graph graph;
protected:
	gui_scrollbar scrollbar;
public:
	guictr_nodes_editor(DAW::Cursor& _cursor, project_t& _project, dragdrop_midifile& _dragdropclip);
	~guictr_nodes_editor();
	void render(NVGcontext* vg);
	void scrollTo(guibase* g);
	void layout();
//private:
//	void updateVisibleTracks();
//public:
//	void updateVisibleTrackContents();

	void onChildLayoutChanged(guibase* g) {
		layout();
	}
//	bool handleKeyInput(KeyEvent& kevt) {
//		return trackView.handleKeyInput(kevt);
//	}
	ivec2 getScrollTotalSize() override {
		ivec2 cs = getSizeContent();
		return cs;
	}
	ivec2 getScrollViewSize() override {
		return graph.size;
	}
	void scrollOffsetChanged(int dir, float offset);
	void setScrollOffset(float offset) {
		this->scrollbar.setScrollOffset(offset);
	}
	float getScrollOffset() {
		return this->scrollbar.scrollOffset;
	}
	void buttonClicked(guibase* _button) {
		if (parent) parent->buttonClicked(_button);
	}
	void reset();
	void refresh();
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
	bool handleKeyInput(KeyEvent& event) override;
	void onTick(AppCtrl* appctrl) override;
	guibase* getFocusedContainer() override {
		return this;
	}
	bool handleMouseScroll(MouseEvent& evt, double xoffset, double yoffset) override {
		return false;
	}
};

class guictr_nodes_splitview : public guictr_base {
public:
	project_t& project;
private:
	guictr_nodes_editor projectView;
	guictr_nodes_editor trackView;
public:
	guictr_nodes_splitview(DAW::Cursor& _cursor, project_t& _project, dragdrop_midifile& _dragdropclip);
	~guictr_nodes_splitview();
	void layout() override;
	void onChildLayoutChanged(guibase* g) override;
	void reset();
	void refresh();
	void buttonClicked(guibase* _button);
};

