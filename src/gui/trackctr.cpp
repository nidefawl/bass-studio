#include <nanovg.h>

#include "gui.h"
#include "guicontainer.h"
#include "exceptions.h"
#include "track.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "trackcontrols.h"
#include "track.h"
#include "track_impl.h"

#include "guicontextmenu_daw.h"


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
		nvgSave(vg);
		g->mixer->render(vg);
		nvgRestore(vg);
	}
	int ySplit = getPosYFirstReturnTrack(project);
	if (ySplit > 0) {
		nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
		for (track_t* g : project.trackCtr) {
			//content
			nvgSave(vg);
			g->mixer->render(vg);
			nvgRestore(vg);
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
	mxrPos = ivec2(0, y);
	int32_t trH = t->hideTrack ? 1 : t->height;
	t->content->size = ivec2(trackView.size.x, trH * TRACK_HEIGHT_STEP);
	int32_t x2 = t->content->left();
	int32_t y2 = t->content->bottom();
	if (!(t->hideTrack || t->hideAutomation)) {
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
	t->mixer->size = ivec2(trackControls.size.x, y2-y);

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
gui_track_automationlane* guictr_tracks::addAutomationLane(track_t* t, automatable_t* at, int32_t paramIdx, bool insertFront) {
	gui_track_automationlane* al = trackView.addAutomationLane(t, at, paramIdx, insertFront);
	t->mixer->addAutomationLane(t, al);
	return al;
}
void guictr_tracks::removeAutomationLane(gui_track_automationlane* al) {
	al->m_track->mixer->removeAutomationLane(al);
	trackView.removeAutomationLane(al);
}
void guictr_tracks::removeAllAutomationLanes(track_t* t, automatable_t* at, int32_t paramIdx) {
	t->mixer->removeAllAutomationLanes(at, paramIdx);
	trackView.removeAllAutomationLanes(t, at, paramIdx);
}
void guictr_tracks::removeAllAutomationLanes(track_t* t, automatable_t* at) {
	t->mixer->removeAllAutomationLanes(at);
	trackView.removeAllAutomationLanes(t, at);
}
void guictr_tracks::removeAllAutomationLanes(track_t* t) {
	t->mixer->removeAllAutomationLanes();
	trackView.removeAllAutomationLanes(t);
}
void guictr_tracks::scrollOffsetChanged(int dir, float offset) {
//	trackView.pos.y = loophandles.bottom()-offset*(contentHeight-size.y);
//	trackControls.pos.y = loophandles.bottom()-offset*(contentHeight-size.y);
	int32_t scrOffset = max(0.0f, offset*(contentHeight-contentViewSize));
	int y = TRACK_HEIGHT_SPACING-scrOffset;
	for (track_t* t : project.trackCtr) {
		assert(t->content != NULL);
		int32_t h = setTrackPosition(t, y, false);
		y += h + TRACK_HEIGHT_SPACING;
	}
}
void guictr_tracks::scrollTo(guibase* g) {
	int32_t y = g->pos.y;
	int32_t scrOffset = max(0.0f, scrollbar.scrollOffset*(contentHeight-contentViewSize));
	scrollbar.scrollVisible(y+scrOffset, g->size.y);
}
void guictr_tracks::layout() {
	bool trackCtrlsLeft = true;
	const int mixerwidth = 380;
	int scrollW = gui_scrollbar::defaultW;

	ivec2 cs = getSizeContent();
	scrollbar.pos = ivec2(cs.x-scrollW, 0);
	scrollbar.size = ivec2(scrollW, cs.y);
	cs.x -= scrollW;
	trackTimeline.pos = ivec2(trackCtrlsLeft?mixerwidth:0, 0);
	trackTimeline.pos = ivec2(trackCtrlsLeft?mixerwidth:0, 0);
	trackTimeline.size = ivec2(cs.x - mixerwidth, 32);
	loophandles.pos = ivec2(trackTimeline.left(), trackTimeline.bottom());
	loophandles.size = ivec2(trackTimeline.size.x, heightTimelineControls);
	trackView.pos = ivec2(trackCtrlsLeft?mixerwidth:0, loophandles.bottom());
	trackControls.pos = ivec2(trackCtrlsLeft?0:cs.x - mixerwidth, loophandles.bottom());
	trackView.size = ivec2(cs.x - mixerwidth, cs.y - loophandles.bottom());
	trackControls.size = ivec2(mixerwidth, trackView.size.y);

	loophandles.clipViewSize = ivec2(trackView.size.x, trackView.size.y+loophandles.size.y);

	double f = scrollbar.toPixels();
	ivec2 csTrackView = trackView.getSizeContent();
	int y = TRACK_HEIGHT_SPACING;
	for (track_t* t : project.trackCtr) {
		assert(t->content != NULL);
		int32_t h = setTrackPosition(t, y, false);
		y += h + TRACK_HEIGHT_SPACING;
	}
	contentHeight = y;
	y = csTrackView.y-TRACK_HEIGHT_SPACING;
//		y = 0;
	auto itMastersTracks = project.tracksBottom.rbegin();
	auto itMastersEnd = project.tracksBottom.rend();
	while (itMastersTracks != itMastersEnd) {
		track_t* t = *itMastersTracks;
		int32_t h = setTrackPosition(t, y, true);
		y -= h;
		assert(t->content != NULL);
		y -= TRACK_HEIGHT_SPACING;
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
	dragdrop_target_indicator& target = MainCtrl::get()->getDragDropTarget();
	bool renderIndicator = target.ptr == this;
	ivec2 indicatorPos = target.targetPos;
	nvgTranslate(vg, 0, trackView.top());
	int ySplit = getPosYFirstReturnTrack(project);
	if (ySplit > 0) {
		nvgSave(vg);
		nvgIntersectScissor(vg, 0, 0, cs.x, ySplit);
		for (track_t* g : project.trackCtr) {
			drawSeperator(vg, theme, g->mixer->bottom()+TRACK_HEIGHT_SPACING_HALF, cs);
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
	if (renderIndicator) {
		nvgSave(vg);
		nvgTranslate(vg, 0, trackView.top());
		horizontalLineAt(this, vg, indicatorPos);
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
gui_track_automationlane* guitrack_editor::addAutomationLane(track_t* t, automatable_t* at, int32_t paramIdx, bool insertFront) {
	assert(t->audio);

	gui_track_automationlane* al = new gui_track_automationlane(t, grid, at, paramIdx);
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
	return al;
}

void guitrack_editor::removeAllAutomationLanes(track_t* t) {
	removeAllAutomationLanes(t, NULL, -1);
}
void guitrack_editor::removeAllAutomationLanes(track_t* t, automatable_t* at) {
	removeAllAutomationLanes(t, at, -1);
}
void guitrack_editor::removeAllAutomationLanes(track_t* t, automatable_t* at, int32_t paramIdx) {
	auto& atLanes = t->subtracks;
	auto it = std::remove_if(atLanes.begin(), atLanes.end(), [this, at, paramIdx] (gui_track_automationlane* al) {
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
void guitrack_editor::removeAutomationLane(gui_track_automationlane* al) {
	assert(al);
	remove(al);
	auto& atLanes = al->m_track->subtracks;
	auto it = std::find(atLanes.begin(), atLanes.end(), al);
	assert(it != atLanes.end());
	atLanes.erase(it);
	delete al;
	int32_t idx = 0;
	for (auto subTr : atLanes) {
		subTr->idx = idx++;
	}
}
int slotFromCoord(project_t& project, track_t* track, ivec2 _pos, ivec2& _posDrop) {
	tracksubcontainer_t* ctrPtr = project.trackTypeCtrs[track->type];
	tracksubcontainer_t& ctr = *ctrPtr;
	int slot = 0;
	for (track_t* track : ctr) {
		auto* gui = track->content;
		if (_pos.y < gui->pos.y + gui->size.y / 2) {
			_posDrop = {gui->pos.x, gui->top()};
			return track->localIdx;
		}
		slot++;
	}
	if (slot >= 0) {
		track_t* track = ctr.back();
		auto* gui = track->content;
		_posDrop = {gui->pos.x, gui->bottom()};
		return track->localIdx+1;
	}
	return -1;
}
namespace {
	void handleTrackEntryDragMove(guibase* parent, project_t& project, track_t* track, ivec2 mousepos) {
		ivec2 posDrop;
		int slot = slotFromCoord(project, track, mousepos, posDrop);
		if (slot >= 0) {
			MainCtrl::get()->getDragDropTarget().set(parent, slot);
			MainCtrl::get()->getDragDropTarget().setPos(posDrop);
		}
	}
	void handleTrackEntryDragRelease(project_t& project, track_t* track, ivec2 mousepos) {
		ivec2 posDrop;
		int targetslot = slotFromCoord(project, track, mousepos, posDrop);
		if (targetslot >= 0 && targetslot != track->localIdx && targetslot != track->localIdx+1) {
			if (targetslot > track->localIdx) targetslot--;
			int distance = targetslot - track->localIdx;
			int destSlot = std::max(0, track->localIdx + distance);
			MainCtrl::get()->getDragDropTarget().reset();
			MainCtrl::get()->trackList.moveTrack(track, destSlot);
			MainCtrl::getGuiTrackCtr()->layout();
			MainCtrl::get()->updateVisibleTrackContents();
		}
	}
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
	assert(t->audio);
	t->content = createTrackGui(t, grid);
	t->content->setZOrder(t->type >= TRACK_TYPE_MIDI ? 0 : 1);
	add(t->content);
//#ifndef NDEBUG
//		for (guibase* child : guis) {
//			gui_track* t = dynamic_cast<gui_track*>(child);
//			assert(t);
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
		g->content->updateVisibleTrackContents(project, grid);
		for (gui_track_automationlane* au : g->subtracks) {
			au->updateVisibleTrackContents(grid);
		}
	}
}
void guitrack_mixers::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_notrack(), evt.mousepos);
}
