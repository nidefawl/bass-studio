#include <nanovg.h>
#include "trackctr.h"
#include "math/seq_math.h"
#include "gui.h"
#include "guicontainer.h"
#include "exceptions.h"
#include "theme.h"
#include "automation.h"
#include "track.h"
#include "trackcontent.h"
#include "trackcontrols.h"
#include "track.h"
#include "track_impl.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "logging.h"

#include "guicontextmenu_daw.h"
namespace GuiConstant {
GuiConstant::constant_t CONST_TRACK_CONTROLS_WIDTH("CONST_TRACK_CONTROLS_WIDTH", 460);
}

void guitrack_mixers::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	ivec2 cs = getSizeContent();
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, cs.x, cs.y);
	nvgFillColor(vg, theme->getColor(GuiColor::COL_GRID_BRT));
	nvgFill(vg);
	for (track_t* g : project.tracksBottom) {
		//content
		dbgassert(g->mixer->isVisible() == g->isVisible());
		if (g->mixer->isVisible()) {
			nvgSave(vg);
			g->mixer->renderGroupHandle(vg);
			g->mixer->render(vg);
			nvgRestore(vg);
		}
	}
	int ySplit = getPosYFirstReturnTrack(project);
	if (ySplit > 0) {
		nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
		for (track_t* t : project.trackMidiAudioCtr) {
			dbgassert(t->mixer != NULL);
			dbgassert(t->mixer->isVisible() == t->isVisible());
			if (t->mixer->isVisible()) {
				nvgSave(vg);
				t->mixer->renderGroupHandle(vg);
				t->mixer->render(vg);
				nvgRestore(vg);
			}
		}
	}

}
void guitrack_mixers::addTrack(track_t* t) {
	if (t->mixer)
		throw applogicexception("expected t->mixer == NULL");
	t->mixer = createTrackGuiMixer(t);
	t->mixer->setZOrder(t->type >= TRACK_TYPE_MIDI ? 0 : 1);
	this->add(t->mixer);
}
void guitrack_mixers::removeTrack(track_t* t) {
	if (t->mixer) {
		this->remove(t->mixer);
		DELETE_PTR(t->mixer)
	}
}
void guitrack_mixers::updateVisibleTrackContents() {
	for (track_t* g : project.trackList) {
		if (!g->mixer) {
			my_printf("NO MIXER ON %s\n", StringAsCStr(g->name));
			continue;
		}
		const bool bVisible = g->isVisible();
		g->mixer->setVisible(bVisible);
	}
}

void drawSeperator(NVGcontext* vg, const guitheme_t* theme, int32_t seperatorY, ivec2& cs) {
	nvgBeginPath(vg);
	nvgMoveTo(vg, 0, seperatorY);
	nvgLineTo(vg, cs.x, seperatorY);
	nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
	nvgStrokeWidth(vg, TRACK_HEIGHT_SPACING);
	nvgStroke(vg);
}
int32_t guictr_tracks::setTrackPosition(track_t* t, int32_t y, bool isBottom) {
	const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
	ivec2& cntPos = t->content->pos;
	ivec2& mxrPos = t->mixer->pos;
	cntPos = ivec2(0, y);
	int lvl = t->getChildLvl();
	int32_t childTrackInsetX = lvl*8;
	mxrPos = ivec2(childTrackInsetX, y);
	int32_t trH = t->hideTrack ? 1 : t->height;
	t->content->size = ivec2(trackView.size.x, trH * TRACK_HEIGHT_STEP);
	int32_t x2 = t->content->left();
	int32_t y2 = t->content->bottom();
	if (!(t->hideTrack || t->hideSubtracks)) {
		for (auto t2 : t->subtracks) {
			int trackheight2 = t2->height * TRACK_HEIGHT_STEP;
			t2->pos = ivec2(x2, y2);
			t2->size = ivec2(t->content->size.x, trackheight2);
			y2 = t2->bottom() + TRACK_HEIGHT_SPACING;
		}
	} else {
		for (auto t2 : t->subtracks) {
			t2->pos = ivec2(x2, y2);
			t2->size = ivec2(0, 0);
		}
	}
	int32_t totalHeight = y2-y;
	t->mixer->size = ivec2(trackControls.size.x-childTrackInsetX, y2-y);

	if (isBottom) {
		cntPos.y -= totalHeight;
		mxrPos.y -= totalHeight;
		for (auto t2 : t->subtracks) {
			t2->pos.y -= totalHeight;
		}
	}
	t->content->positionChanged();
	for (auto t2 : t->subtracks) {
		t2->positionChanged();
	}
	return totalHeight;
}

void guictr_tracks::showAutomationLane(track_t* tr, automatable_t* at, int32_t paramIdx) {
	tr->audio->selectedAutomationCtr = at;
	tr->audio->selectedAutomationParam = paramIdx;
	updateVisibleTrackContents();
}
void guictr_tracks::addSubTrack(track_t* t, gui_track_subtrack* subtrack, bool insertFront) {
	trackView.addSubtrack(t, subtrack, insertFront);
	t->mixer->addSubtrackMixer(t, subtrack);
}
gui_track_automationlane* guictr_tracks::addAutomationLane(track_t* t, automatable_t* at, int32_t paramIdx, bool insertFront) {
	gui_track_automationlane* al = new gui_track_automationlane(t, grid, at, paramIdx);
	addSubTrack(t, al, insertFront);
	return al;
}
void guictr_tracks::removeAutomationLane(gui_track_automationlane* al) {
	al->m_track->mixer->removeSubtrackMixer(al);
	trackView.removeSubtrack(al);
}
void guictr_tracks::removeAllAutomationLanes(track_t* t, automatable_t* at, int32_t paramIdx) {
	t->mixer->removeAllAutomationLanes(at, paramIdx);
	trackView.removeAllAutomationLanes(t, at, paramIdx);
}
void guictr_tracks::removeAllAutomationLanes(track_t* t, automatable_t* at) {
	t->mixer->removeAllAutomationLanes(at);
	trackView.removeAllAutomationLanes(t, at);
}
void guictr_tracks::removeAllSubtracks(track_t* t) {
	t->mixer->removeAllSubtracks();
	trackView.removeAllSubtracks(t);
}
void guictr_tracks::scrollOffsetChanged(int dir, float offset) {
//	trackView.pos.y = loophandles.bottom()-offset*(contentHeight-size.y);
//	trackControls.pos.y = loophandles.bottom()-offset*(contentHeight-size.y);
	int32_t scrOffset = math::max(0.0f, offset*(contentHeight-contentViewSize));
	int y = TRACK_HEIGHT_SPACING-scrOffset;
	for (track_t* t : project.trackMidiAudioCtr) {
		dbgassert(t->content != NULL);
		if (t->isVisible()) {
			int32_t h = setTrackPosition(t, y, false);
			y += h + TRACK_HEIGHT_SPACING;
		}
	}
}
void guictr_tracks::scrollTo(guibase* g) {
	int32_t y = g->pos.y;
	int32_t scrOffset = math::max(0.0f, scrollbar.scrollOffset*(contentHeight-contentViewSize));
	scrollbar.scrollVisible(y+scrOffset, g->size.y);
}
void guictr_tracks::layout() {
	bool trackCtrlsLeft = true;
	const int32_t trackControlsWidth = theme->get(GuiConstant::CONST_TRACK_CONTROLS_WIDTH);
	int scrollW = gui_scrollbar::defaultW;

	ivec2 cs = getSizeContent();
	scrollbar.pos = ivec2(cs.x-scrollW, 0);
	scrollbar.size = ivec2(scrollW, cs.y);
	cs.x -= scrollW;
	trackTimeline.pos = ivec2(trackCtrlsLeft?trackControlsWidth:0, 0);
	trackTimeline.pos = ivec2(trackCtrlsLeft?trackControlsWidth:0, 0);
	trackTimeline.size = ivec2(cs.x - trackControlsWidth, 32);
	loophandles.pos = ivec2(trackTimeline.left(), trackTimeline.bottom());
	loophandles.size = ivec2(trackTimeline.size.x, heightTimelineControls);
	trackView.pos = ivec2(trackCtrlsLeft?trackControlsWidth:0, loophandles.bottom());
	trackControls.pos = ivec2(trackCtrlsLeft?0:cs.x - trackControlsWidth, loophandles.bottom());
	trackView.size = ivec2(cs.x - trackControlsWidth, cs.y - loophandles.bottom());
	trackControls.size = ivec2(trackControlsWidth, trackView.size.y);

	loophandles.clipViewSize = ivec2(trackView.size.x, trackView.size.y+loophandles.size.y);

	double f = scrollbar.toPixels();
	ivec2 csTrackView = trackView.getSizeContent();
	int y = TRACK_HEIGHT_SPACING;
	for (track_t* t : project.tracksVisibleFlat) {
		dbgassert(t->content != NULL);
		if (t->isVisible()) {
			int32_t h = setTrackPosition(t, y, false);
			y += h + TRACK_HEIGHT_SPACING;
		}
	}
	contentHeight = y;
	y = csTrackView.y-TRACK_HEIGHT_SPACING;
//		y = 0;
	auto itMastersTracks = project.tracksBottom.rbegin();
	auto itMastersEnd = project.tracksBottom.rend();
	while (itMastersTracks != itMastersEnd) {
		track_t* t = *itMastersTracks;
		if (t->isVisible()) {
			int32_t h = setTrackPosition(t, y, true);
			y -= h;
			dbgassert(t->content != NULL);
			y -= TRACK_HEIGHT_SPACING;
		}
		itMastersTracks++;
	}
	contentViewSize = y;
	if (contentHeight >= contentViewSize) {
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		contentHeight += TRACK_HEIGHT_STEP*4;
	}
	scrollbar.scrollTo(f);

	scrollOffsetChanged(1, scrollbar.scrollOffset);
	for (guibase* gui : guis) {
		gui->layout();
	}
	MainCtrl::get()->updateGrid();
}
void horizontalLineAt(guictr_base* gui, NVGcontext* vg, ivec2 posHL) {
	nvgLineCap(vg, NVGlineCap::NVG_ROUND);
	nvgBeginPath(vg);
	nvgMoveTo(vg, 4, posHL.y);
	int32_t width = gui->getSizeContent().x;
	nvgLineTo(vg, width - 4, posHL.y);
	nvgStrokeColor(vg, G_MOVE_HIGHLIGHT);
	nvgStrokeWidth(vg, 4.0);
	nvgStroke(vg);
	nvgLineCap(vg, NVGlineCap::NVG_BUTT);
}
void guictr_tracks::render(NVGcontext* vg) {
	if (isBackgroundRendered()){
		renderBackground(vg);
	}
	ivec2 cs = getSizeContent();
	ivec2 cp = getPosContent();
	if (cs.y <= 0 || cs.x <= 0) {
		return;
	}
	nvgIntersectScissor(vg, cp.x, cp.y, cs.x, cs.y);
	nvgTranslate(vg, cp.x, cp.y);
	nvgSave(vg);
		trackView.render(vg);
	nvgRestore(vg);
	nvgSave(vg);
		trackControls.render(vg);
	nvgRestore(vg);
	nvgSave(vg);
		trackTimeline.render(vg);
	nvgRestore(vg);

	nvgSave(vg);
	dragdrop_target_indicator_t& target = MainCtrl::get()->getDragDropTarget();
	nvgTranslate(vg, 0, trackView.top());
	int ySplit = getPosYFirstReturnTrack(project);
	if (ySplit > 0) {
		nvgSave(vg);
		nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
		for (track_t* t : project.trackMidiAudioCtr) {
			dbgassert(t->mixer != NULL);
			drawSeperator(vg, theme, t->mixer->bottom()+TRACK_HEIGHT_SPACING_HALF, cs);
		}
		nvgRestore(vg);
	}
	if (project.tracksBottom.size()) {
		if (ySplit > 0) {
			nvgIntersectScissor(vg, 0, ySplit, cs.x, trackView.size.y-ySplit);
		} else {
			nvgIntersectScissor(vg, 0, 0, cs.x, trackView.size.y);
		}
		for (track_t* g : project.tracksBottom) {
			drawSeperator(vg, theme, g->mixer->top()-TRACK_HEIGHT_SPACING_HALF, cs);
		}
	}
	nvgRestore(vg);
	if (target.src == this && target.dst) {
		nvgSave(vg);
		nvgTranslate(vg, 0, trackView.top());
		auto& dragDropTarget = MainCtrl::get()->getDragDropTarget();
		if (dragDropTarget.src) {
			guitrack_mixers* trackMixers = static_cast<guitrack_mixers*>(dragDropTarget.src);
//			dragDropTarget.src->renderWidgetBorderPosSize(vg, flags, pos, size)
			int n = trackMixers->theme->get(GuiConstant::CONST_GUI_INSET_WIDGET_BG);
			auto bgPos = /*trackMixers->getPosContent()+*/ivec2(n);
			auto bgSize = trackMixers->getSizeContent()-ivec2(n*2);
			if (bgSize.x > 0 && bgSize.y > 0) {
				nvgGlobalAlpha(vg, 0.5f);
				nvgBeginPath(vg);
				nvgRect(vg, bgPos.x, bgPos.y, bgSize.x, bgSize.y);
				if (dragdrop_target_indicator_t::target_area == target.type) {
					nvgPathWinding(vg, NVGwinding::NVG_CW);
					nvgRect(vg, target.dst->pos.x, target.dst->pos.y, target.dst->size.x, target.dst->size.y);
					nvgPathWinding(vg, NVGwinding::NVG_CCW);
				}
				auto color = getBackgroundColor(0);
				nvgFillColor(vg, color);
				nvgFill(vg);
				nvgGlobalAlpha(vg, 1.0f);
			}
		}
		const int titleHeight = theme->get(GuiConstant::CONST_TRACK_HEIGHT_TITLE);
		ivec2 indicatorPos = target.targetPos;
		horizontalLineAt(this, vg, indicatorPos);

		int fontScale = titleHeight;
		String str = StringFormat("%s[%d]", StringAsCStr(static_cast<gui_track_controls*>(target.dst)->m_track->name), target.slotIdx);
		renderCenteredMultilineText(vg, theme, str, fontScale, GuiColor::COL_LABEL_ACTIVE, indicatorPos+ivec2(size.x, 0), ivec2(titleHeight*30, titleHeight*2));

		nvgRestore(vg);
	}

	nvgBeginPath(vg);
	nvgMoveTo(vg, trackControls.left(), trackControls.top());
	nvgLineTo(vg, trackControls.left(), trackControls.bottom());
	nvgStrokeColor(vg, theme->getColor(GuiColor::COL_LINE_SEPERATOR));
	nvgStrokeWidth(vg, 3);
	nvgStroke(vg);




	nvgSave(vg);
	loophandles.render(vg);
	nvgRestore(vg);
	nvgSave(vg);
	scrollbar.render(vg);
	nvgRestore(vg);

	if (trackView.size.x > 0) {
		nvgIntersectScissor(vg, trackView.pos.x, 0, trackView.size.x, cs.y);
		nvgTranslate(vg, trackView.pos.x, 0);
		tick_t pos = project.playbackPos;
//		if (project.loopEnabled) {
//			if (pos > project.loopStart) {
//				pos = project.loopStart + (pos - project.loopStart) % project.loopLen;
//			}
//		}
		float playBackX = (float) grid.tickToScreenD(pos);
		if (playBackX > -4.0f && playBackX < cs.x + 4.0f) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, playBackX, 0);
			nvgLineTo(vg, playBackX, cs.y);
			nvgStrokeColor(vg, GUI_COLOR(120));
			nvgStrokeWidth(vg, 3);
			nvgStroke(vg);
			nvgBeginPath(vg);
			nvgMoveTo(vg, playBackX, 0);
			nvgLineTo(vg, playBackX, cs.y);
			nvgStrokeColor(vg, GUI_COLOR(250));
			nvgStrokeWidth(vg, 1);
			nvgStroke(vg);
		}
//		nvgIntersectScissor(vg, 0, 0, trackView.size.x, trackView.size.y);
//		nvgTranslate(vg, 0, trackTimeline.bottom());

//		double playBackX = grid.tickToScreenD(MainCtrl::get()->playbackPos);
//		if (playBackX > -4 && playBackX < cs.x+4) {
//			nvgBeginPath(vg);
//			nvgMoveTo(vg, playBackX, 0);
//			nvgLineTo(vg, playBackX, cs.y);
//			nvgStrokeColor(vg, GUI_COLOR(120));
//			nvgStrokeWidth(vg, 3);
//			nvgStroke(vg);
//			nvgBeginPath(vg);
//			nvgMoveTo(vg, playBackX, 0);
//			nvgLineTo(vg, playBackX, cs.y);
//			nvgStrokeColor(vg, GUI_COLOR(250));
//			nvgStrokeWidth(vg, 1);
//			nvgStroke(vg);
//		}
	}
}
void guitrack_editor::addSubtrack(track_t* t, gui_track_subtrack* al, bool insertFront) {
	if (insertFront) {
		t->subtracks.insert(t->subtracks.begin(), al);
	} else {
		t->subtracks.push_back(al);
	}
	int32_t idx = 0;
	for (auto subTr : t->subtracks) {
		subTr->idx = idx++;
	}
	al->setZOrder(t->type >= TRACK_TYPE_MIDI ? 0 : 1);
	add(al);
}

void guitrack_editor::removeAllSubtracks(track_t* t) {
	auto& atLanes = t->subtracks;
	for (auto at : atLanes) {
		remove(at);
		delete at;
	}
	atLanes.clear();
}
void guitrack_editor::removeAllAutomationLanes(track_t* t, automatable_t* at) {
	removeAllAutomationLanes(t, at, -1);
}
void guitrack_editor::removeAllAutomationLanes(track_t* t, automatable_t* at, int32_t paramIdx) {
	auto& atLanes = t->subtracks;
	auto it = std::remove_if(atLanes.begin(), atLanes.end(), [this, at, paramIdx] (gui_track_subtrack* al) {
		if (al->subtrackType() != gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
			return false;
		}
		if ((at == NULL || al->at == at) && (paramIdx < 0 || al->param == paramIdx)) {
			remove(al);
			delete al;
			return true;
		}
		return false;
	});
	atLanes.erase(it, atLanes.end());

	int32_t idx = 0;
	for (auto subTr : atLanes) {
		subTr->idx = idx++;
	}
}
void guitrack_editor::removeSubtrack(gui_track_automationlane* al) {
	dbgassert(al);
	remove(al);
	auto& atLanes = al->m_track->subtracks;
	auto it = std::find(atLanes.begin(), atLanes.end(), al);
	dbgassert(it != atLanes.end());
	atLanes.erase(it);
	delete al;
	int32_t idx = 0;
	for (auto subTr : atLanes) {
		subTr->idx = idx++;
	}
}
class gui_track_drop_position_t {
public:
	enum drop_type {
		none, track_on, track_before, track_after
	};
	int slot = 0;
	track_t *droppedTrack;
	drop_type droptype=none;
	ivec2 pos{};
};
gui_track_drop_position_t slotFromCoord(const project_t& project, track_t* const track, const ivec2 _pos) {
	using drop_type = gui_track_drop_position_t::drop_type;

	auto& trackList = project.trackList;
	int minDistDragPoint = std::numeric_limits<int32_t>::max();
	gui_track_drop_position_t minSlot{0, nullptr, drop_type::none, {0, 0}};
	auto checkDropPoint = [](int32_t minY, int32_t maxY, int mouseY) -> int32_t {
		if (mouseY >= minY && mouseY < maxY) {
			return math::abs(minY + (maxY - minY) / 2 - mouseY);
		}
		return -1;
	};
	const auto itcBegin = trackList.crbegin();
	const auto itcEnd = trackList.crend();
	for (auto it = itcBegin; it != itcEnd; it++) {
		int32_t slotIdx = itcEnd - it - 1;
		track_t* track = *it;
		const int dropMaxDistance = 32;
		auto* gui = track->content;
		int32_t distDragPoint = checkDropPoint(gui->pos.y - dropMaxDistance, gui->pos.y + dropMaxDistance, _pos.y);
		if (distDragPoint >= 0 && distDragPoint < minDistDragPoint) {
			minDistDragPoint = distDragPoint;
			minSlot = {slotIdx, track, drop_type::track_before, {gui->pos.x, gui->pos.y}};
		}
		distDragPoint = checkDropPoint(gui->pos.y + dropMaxDistance, gui->pos.y + gui->size.y - dropMaxDistance, _pos.y);
		if (distDragPoint >= 0 && distDragPoint < minDistDragPoint) {
			minDistDragPoint = distDragPoint;
			minSlot = {slotIdx, track, drop_type::track_on, {gui->pos.x, gui->pos.y+gui->size.y/2}};
		}
		if (track->children.empty()) {
			distDragPoint = checkDropPoint(gui->pos.y + gui->size.y - dropMaxDistance, gui->pos.y + gui->size.y + dropMaxDistance, _pos.y);
			if (distDragPoint >= 0 && distDragPoint < minDistDragPoint) {
				minDistDragPoint = distDragPoint;
				minSlot = {slotIdx, track, drop_type::track_after, {gui->pos.x, gui->pos.y+gui->size.y}};
			}
		}
	}
	return minSlot;
}
namespace {
	using drop_type = gui_track_drop_position_t::drop_type;
	void handleTrackEntryDragMove(guibase* parent, project_t& project, track_t* track, ivec2 mousepos) {
		MainCtrl::get()->getDragDropTarget().reset();
		gui_track_drop_position_t slot = slotFromCoord(project, track, mousepos);

		track_tree_pos_t treePos{};
		dbgassert(slot.droptype == drop_type::none || slot.droppedTrack);
		switch (slot.droptype) {
		case drop_type::none:
			return;
		case drop_type::track_on:
			//insert into slot.droppedTrack at end
			treePos.parent = slot.droppedTrack;
			treePos.treeIdx = slot.droppedTrack->children.size() ? slot.droppedTrack->children.back()->childIdxTree : 0;
			break;
		case drop_type::track_before:
			//insert into slot.droppedTrack->parent before slot.droppedTrack
			treePos.parent = slot.droppedTrack->parent;
			treePos.treeIdx = slot.droppedTrack->childIdxTree;
			break;
		case drop_type::track_after:
			//insert into slot.droppedTrack->parent after slot.droppedTrack
			treePos.parent = slot.droppedTrack->parent;
			{
				int idx = slot.droppedTrack->childIdxTree+1;
				auto p = slot.droppedTrack->parent;
				while (p && idx == p->children.size()) {
					idx = p->childIdxTree+1;
					p = p->parent;
				}
				treePos.parent = p;
				treePos.treeIdx = idx;
			}
			break;
		default:
			dbgassert(0);
			return;
		}

		dragdrop_target_indicator_t target;
		switch (slot.droptype) {
		case drop_type::track_on:
			target = { dragdrop_target_indicator_t::target_area, treePos.treeIdx, parent, treePos.parent ? treePos.parent->mixer : parent, slot.droppedTrack->mixer->pos+ivec2(0, slot.droppedTrack->mixer->size.y/2) };
			break;
		case drop_type::track_before:
			target = { dragdrop_target_indicator_t::target_line, treePos.treeIdx, parent, treePos.parent ? treePos.parent->mixer : parent, slot.droppedTrack->mixer->pos+ivec2(0,2) };
			break;
		case drop_type::track_after:
			target = { dragdrop_target_indicator_t::target_line, treePos.treeIdx, parent, treePos.parent ? treePos.parent->mixer : parent, slot.droppedTrack->mixer->pos+ivec2(0, slot.droppedTrack->mixer->size.y-2) };
			break;
		case drop_type::none:
			return;
		default:
			dbgassert(0);
			return;
		}
		MainCtrl::get()->getDragDropTarget() = target;
	}
	void handleTrackEntryDragRelease(project_t& project, track_t* track, ivec2 mousepos) {
		dbgassert(project.trackList.size());
		gui_track_drop_position_t slot = slotFromCoord(project, track, mousepos);
		track_tree_pos_t treePos{};
		dbgassert(slot.droptype == drop_type::none || slot.droppedTrack);
		switch (slot.droptype) {
		case drop_type::none:
			return;
		case drop_type::track_on:
			//insert into slot.droppedTrack at end
			treePos.parent = slot.droppedTrack;
			treePos.treeIdx = slot.droppedTrack->children.size() ? slot.droppedTrack->children.back()->childIdxTree : 0;
			break;
		case drop_type::track_before:
			//insert into slot.droppedTrack->parent before slot.droppedTrack
			treePos.parent = slot.droppedTrack->parent;
			treePos.treeIdx = slot.droppedTrack->childIdxTree;
			break;
		case drop_type::track_after:
			//insert into slot.droppedTrack->parent after slot.droppedTrack
			treePos.parent = slot.droppedTrack->parent;
			{
				int idx = slot.droppedTrack->childIdxTree+1;
				auto p = slot.droppedTrack->parent;
				while (p && idx == p->children.size()) {
					idx = p->childIdxTree+1;
					p = p->parent;
				}
				treePos.parent = p;
				treePos.treeIdx = idx;
			}
			break;
		default:
			dbgassert(0);
			return;
		}
		if (TRACKTYPE_TO_CTR(slot.droppedTrack->type) != TRACKTYPE_TO_CTR(track->type)) {
			log_printf("Cannot move there\n", 0);
			return;
		}
		treePos.trackTypeCtr = TRACKTYPE_TO_CTR(track->type);
		std::vector<track_t*> selectedTracks;
		selectedTracks.push_back(track);
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		bool failed = !MainCtrl::get()->trackList.moveTracks(selectedTracks, treePos);
		String strTarget = "<root>";
		if (treePos.parent) {
			strTarget = treePos.parent->name;
		}
		log_printf("Moving %d tracks to %s[%d] %s\n", selectedTracks.size(), StringAsCStr(strTarget), treePos.treeIdx, failed ? "Failed" : "Success");
		MainCtrl::getGuiTrackCtr()->layout();
		MainCtrl::get()->updateVisibleTrackContents();
//			//TODO: edithistory entry
	}
}
bool guitrack_editor::mouseHitTest(ivec2 v, MouseHitEvt& evt) {
	if (this->contains(v)) {
		if (evt.type == MouseHitType::MOUSE_DRAGDROP_OBJECT) {
			evt.requestFocus(this);
			return true;
		}
		if (evt.type == MOUSE_DRAGDROP_CLIP) {
			evt.requestFocus(this);
			return true;
		}
		ivec2 localMouse = this->toContainerSpace(v);
		for (guibase* gui : guis) {
			if (gui->mouseHitTest(localMouse, evt)) {

				// respect z-order, not an actual hit
				if (!evt.getGuiHit()) {
					break;
				}

				return true;
			}
		}
		evt.requestFocus(this);
		return true;
	}
	return false;
}
void guitrack_editor::trackEntryDragMove(gui_track* g, ivec2 mousepos) {
	handleTrackEntryDragMove(parent, project, g->getTrack(), mousepos);
}
void guitrack_mixers::trackEntryDragMove(gui_track* g, ivec2 mousepos) {
	handleTrackEntryDragMove(parent, project, g->getTrack(), mousepos);
}
void guitrack_editor::trackEntryDragRelease(gui_track* g, ivec2 mousepos) {
	handleTrackEntryDragRelease(project, g->getTrack(), mousepos);
}
void guitrack_mixers::trackEntryDragRelease(gui_track* g, ivec2 mousepos) {
	handleTrackEntryDragRelease(project, g->getTrack(), mousepos);
}
void guitrack_editor::addTrack(track_t* t) {
	if (t->content)
		throw applogicexception("expected t->content == NULL");
	dbgassert(t->audio);
	//
	t->content = createTrackGui(t, grid);
	t->content->setZOrder(t->type >= TRACK_TYPE_MIDI ? 0 : 1);
	add(t->content);
	//TODO: sort and render guis by track->idx
//#ifndef NDEBUG
//		for (guibase* child : guis) {
//			gui_track* t = dynamic_cast<gui_track*>(child);
//			dbgassert(t);
//		}
//		int idx = 0;
//		for (guibase* child : guis) {
//			gui_track* t = dynamic_cast<gui_track*>(child);
//			my_printf("idx %d = %s\n", idx, StringAsCStr(t->m_track->name));
//			idx++;
//		}
//#endif
}
void guitrack_editor::removeTrack(track_t* t) {
	if (t->content) {
		t->content->destroyGuis();
		remove(t->content);
		DELETE_PTR(t->content)
	}
	if (t->subtracks.size()) {
		for (auto str : t->subtracks) {
			remove(str);
		}
	}
}
void guitrack_editor::layout() {
	for (guibase* gui : guis) {
		gui->layout();
	}
}
void guitrack_editor::updateVisibleTrackContents() {
	for (track_t* g : project.trackList) {
		if (!g->content) {
			my_printf("NO CONTENT ON %s\n", StringAsCStr(g->name));
			continue;
		}
		const bool bVisible = g->isVisible();
		g->content->setVisible(bVisible);
		if (bVisible) {
			g->content->updateVisibleTrackContents(project, grid);
			for (gui_track_subtrack* au : g->subtracks) {
				au->updateVisibleTrackContents(grid);
			}
		}
	}
}
void guitrack_mixers::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_notrack(), evt.mousepos);
}
