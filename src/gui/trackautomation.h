#pragma once
#include <vector>
#include "seq_time.h"
#include "track.h"
#include "trackctr.h"
#include "automation.h"
#include <glm/vec2.hpp>
#include "leak_detect.h"
using glm::vec2;
using glm::ivec2;

class gui_track_automation : public guictr_base {
public:
	enum dragmode {
		drag_none = 0,
		drag_segment,
		drag_node,
		drag_selection,
		drag_empty,
	};
	struct path_segment_t {
		vec2 pt1;
		vec2 pt2;
		int32_t idx;
	};
	struct path_segment_dataidx_t {
		int32_t idx1;
		int32_t idx2;
	};
	struct hit_result {
		dragmode mode;
		int idx;
		float dist;
		int32_t numPoints = 0;
	};
protected:
	track_t* const m_track;
private:
	const NVGcolor color = G_BLUE2;
	const NVGcolor color2 = mulSatBright(color, 0.6f, 0.8f);
	const NVGcolor colorHL = G_PURPLE;
	const NVGcolor colorHL2 = mulSatBright(colorHL, 0.6f, 0.8f);
	const float radiusHandle = 2.5f;
	const float radiusHandleHL = 3.5f;
	const float lineWidth = 2.5f;

	scaled_grid& grid;
	automatable_t*& at;
	int32_t& param;
	automation_view_t data;
	std::vector<vec2> cachedShape;
	std::vector<path_segment_t> segments;
//	std::vector<path_segment_dataidx_t> segmentDataIdx;
	std::vector<automation_point_t> dataPointsCopy;
	std::vector<automation_point_t> dataPointsEdited;
	int32_t segmentDataPtOffset = 0;
	hit_result dragged = {drag_none, -1, 0};
	bool canSimplify = false;

	hit_result hitTest(vec2 mpos);

public:
	gui_track_automation(track_t* _track, scaled_grid& _grid, automatable_t*& _at, int32_t& _param)
	  : guictr_base(), m_track(_track), grid(_grid), at(_at), param(_param) {
		padding = 8;
	}
	void setData() {
		data.targetParam = param;
		automatable_t* automatable = this->at;
		automation_t* automation = NULL;
		if (automatable) {
			automation = automatable->getAutomation(param);
		}
		if (automation) {
			data.points = automation->points;
		} else {
			data.points.clear();
		}
	}
	ivec2 paddingTL(int _padding) override {
		return ivec2(0, _padding);
	}
	ivec2 paddingBR(int _padding) override {
		return ivec2(0, _padding);
	}
	path_segment_t* getSegmentSafe(int32_t idx) {
		if (idx >= 0 && idx < segments.size()) {
			return &segments[idx];
		}
		return NULL;
	}
	vec2* getPathPointSafe(int32_t idx) {
		if (idx >= 0 && idx < cachedShape.size()) {
			return &cachedShape[idx];
		}
		return NULL;
	}
	void trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) override;
	void trackViewDragMove(guitrack_editor* view, MouseEvent& evt) override;
	void trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) override;
	bool trackViewDoubleClick(guitrack_editor* view, MouseEvent& evt) override;
	void postEdit();
	void handleDraggedBegin(MouseEvent& evt) override {
		MainCtrl::get()->setSelectedTrack(m_track);
		evt.relMousepos += getPosContent();
		parent->handleDraggedBegin(evt);
	}

	void handleDraggedMove(MouseEvent& evt) override {
		evt.relMousepos += getPosContent();
		parent->handleDraggedMove(evt);
	}

	void handleDraggedRelease(MouseEvent& evt) override {
		evt.relMousepos += getPosContent();
		parent->handleDraggedRelease(evt);
	}

	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;

	void updateVisibleTrackContents(scaled_grid& grid);
	void layout() override {

	}

	bool handleKeyInput(KeyEvent& kevt) override {
		return parent->handleKeyInput(kevt);
	}
	void render(NVGcontext* vg) override;
};
