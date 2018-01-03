#include "trackctr.h"
#include "trackcontent.h"
#include "trackcontrols.h"
#include "guicontextmenu.h"
#include <glm/vec2.hpp>
using glm::vec2;
using glm::ivec2;


void guitrack_mixers::render(NVGcontext* vg) {
	if (!setScissorTransform(vg)) {
		return;
	}
	ivec2 cs = getSizeContent();
	nvgBeginPath(vg);
	nvgRect(vg, 0, 0, cs.x, cs.y);
	nvgFillColor(vg, g_guiColors[COL_GRID_BRT]);
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

void guictr_tracks::drawSeperator(NVGcontext* vg, track_t* g, ivec2& cs) {
	//draw (seperator) line at top or bottom of track
	int seperatorY = g->mixer->pos.y;
	if (g->type >= TRACK_TYPE_MIDI) {
		seperatorY += g->mixer->size.y;
	}
	nvgBeginPath(vg);
	nvgMoveTo(vg, 0, seperatorY);
	nvgLineTo(vg, cs.x, seperatorY);
	nvgStrokeColor(vg, g_guiColors[COL_LINE_SEPERATOR]);
	nvgStrokeWidth(vg, TRACK_HEIGHT_SPACING);
	nvgStroke(vg);
}
void guictr_tracks::setTrackPosition(track_t* t, int32_t trackheight, int32_t y) {
	t->content->pos.x = 0;
	t->content->pos.y = y;
	t->mixer->pos.x = 0;
	t->mixer->pos.y = y;
	t->content->size = ivec2(trackView.size.x, trackheight);
	t->mixer->size = ivec2(trackControls.size.x, trackheight);
}
void guitrack_editor::addTrack(track_t* t) {
	if (t->content)
		throw applogicexception("expected t->content == NULL");
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
}
void guitrack_editor::updateVisibleTrackContents() {
	for (track_t* g : project.trackList) {
		if (!g->content) {
			my_printf("NO CONTENT ON %s\n", StringAsCStr(g->name));
			continue;
		}
		g->content->updateVisibleTrackContents(grid);
	}
}
void guitrack_mixers::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_notrack(), evt.mousepos);
}
