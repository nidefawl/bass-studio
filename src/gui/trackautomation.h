#pragma once
#include <vector>
#include "seq_time.h"
#include "trackcontent.h"
#include "automation.h"
#include <glm/vec2.hpp>
#include "leak_detect.h"
using glm::vec2;
using glm::ivec2;

struct automation_clipboard_t {
	tick_t start;
	tick_t len;
	std::vector<automation_point_t> dataPoints;
};


class gui_track_automation : public gui_track {
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
private:
	const NVGcolor color = G_BLUE2;
	const NVGcolor color2 = mulSatBright(color, 0.6f, 0.8f);
	const NVGcolor colorHL = G_PURPLE;
	const NVGcolor colorHL2 = mulSatBright(colorHL, 0.6f, 0.8f);
	const float radiusHandle = 2.5f;
	const float radiusHandleHL = 3.5f;
	const float lineWidth = 2.5f;

	trackdata_automation_t& data;
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
	gui_track_automation(track_t* _track) : gui_track(_track), data(_track->getAutomation()) {
		padding = 8;
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

	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;

	void updateVisibleTrackContents(scaled_grid& grid) override;
	void layout() override {

	}

	bool handleKeyInput(KeyEvent& kevt) override {
		return parent->handleKeyInput(kevt);
	}
	void render(NVGcontext* vg) override;
};
