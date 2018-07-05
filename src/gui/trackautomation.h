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
		std::vector<int32_t> points;
		int32_t dataOffset;
	};
	struct hit_result {
		dragmode mode;
		int dataPt;
		int segidx;
		float dist;
		int32_t numPoints = 0;
		hit_result(dragmode _mode, int _dataPt, int _segidx, float _dist, int32_t _numPoints = 0)
		  : mode(_mode), dataPt(_dataPt), segidx(_segidx), dist(_dist), numPoints(_numPoints) {

		}
	};
protected:
	track_t* const m_track;
private:
	const NVGcolor color = G_BLUE2;
	const NVGcolor colorInactive = rgbaToNvg(0xAAAAAAAA);
	const NVGcolor color2 = mulSatBright(color, 0.6f, 0.8f);
	const NVGcolor colorHL = G_PURPLE;
	const NVGcolor colorHL2 = mulSatBright(colorHL, 0.6f, 0.8f);
	const float radiusHandle = 2.5f;
	const float radiusHandleHL = 3.5f;
	const float lineWidth = 2.5f;

	scaled_grid& grid;
	automatable_t*& at;
	int32_t& param;
	int32_t& idx;
	automation_view_t data;
	std::vector<vec2> cachedShape;
	std::vector<path_segment_t> segments;
	std::vector<automation_point_t> dataPointsCopy;
	std::vector<automation_point_t> dataPointsEdited;
	hit_result dragged = {drag_none, -1, -1, 0};
	bool canSimplify = false;

	hit_result hitTest(vec2 mpos);

public:
	gui_track_automation(track_t* _track, scaled_grid& _grid, automatable_t*& _at, int32_t& _param, int32_t& _idx)
	  : guictr_base(), m_track(_track), grid(_grid), at(_at), param(_param), idx(_idx) {
		padding = 8;
	}
	float getDstVal();
	void setDstVal(float f);
	void setData() {
		if (dragged.mode != dragmode::drag_none)
			return;
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
	bool isActive() {
		if (this->at) {
			automation_t* automation = this->at->getAutomation(param);
			return automation && automation->isActive();
		}
		return false;
	}
	ivec2 paddingTL(int _padding) override {
		return ivec2(0, _padding);
	}
	ivec2 paddingBR(int _padding) override {
		return ivec2(0, _padding);
	}
	path_segment_t* getSegmentSafe(int32_t idx) {
		if (idx >= 0 && idx < (int32_t)segments.size()) {
			return &segments[idx];
		}
		return NULL;
	}
	vec2* getPathPointSafe(int32_t idx) {
		if (idx >= 0 && idx < (int32_t)cachedShape.size()) {
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
