#include "track.h"
#include "trackcontent.h"
#include "trackctr.h"
#include "guicontextmenu.h"
#include "button.h"
#include "event.h"
#include "../host/vst_plugin.h"
#include "track_audiodata.h"
#include "trackautomation.h"
#include "dropdown.h"
#include "leak_detect.h"
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
using glm::vec2;
using glm::ivec2;

float noteToScreen(float note, float scale, float offset, float sizeY) {
	float offsetKey = note * scale;
	float rel = offsetKey - offset;
	return (sizeY) - rel;
}
void gui_clip::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
/*static*/ void gui_clip::renderClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, ivec2 pos, ivec2 size) {
	if (cl->len <= 0) {
		return;
	}
	NVGcolor color = rgbToNvg(cl->rgb);
	nvgBeginPath(vg);
	nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
	nvgFillColor(vg, color);
	nvgFill(vg);
	nvgStrokeColor(vg, G_BLACK);
	nvgStrokeWidth(vg, 1.f);
	nvgStroke(vg);
	if (cl->name.length()) {
		setFont(vg, (int) (HEIGHT_CLIP_TITLE * 0.95), getContrastFontColor(cl->rgb), G_TITLE_ALIGN);
		renderText(vg, pos.x + INSET_TITLE, pos.y + HEIGHT_CLIP_TITLE / 2, size.x-INSET_TITLE*3, StringAsCStr(cl->name));
	}
	ivec2 posContents = ivec2(pos.x, pos.y+HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
	ivec2 sizeContents = ivec2(size.x, size.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);

	tick_t clipLen = cl->len;
	float numBars = clipLen / (float) TICKS_BAR;
	float barSize = sizeContents.x / (float) numBars;
	if (sizeContents.x > 0 && sizeContents.y > 0) {
		nvgSave(vg);
		nvgTranslate(vg, posContents.x, posContents.y);
		nvgBeginPath(vg);

		clip_notes_t& notesView = cl->getNoteViewRender();
		clip_notes_t& notesPlay = cl->getNoteViewPlayback();
	//	clip_notes_t notesPlay;
	//	cl->getNotesView(0, cl->len, notesPlay, true);
		for (int i = 0; i < (tr?(tr->idx%2)+1:1); i++) {
			int32_t rgbNote = i == 0 ? 0xff9933 : 0x33ff33;
			int32_t rgbNoteOverlap = i == 0 ? 0x0000ff : 0xff00ff;
			clip_notes_t& notes = i == 0 ? notesView : notesPlay;
			if (!notes.empty()) {
				note_t minN = notesView.minNote;
				note_t maxN = notesView.maxNote;
				int32_t numNotes = max((int32_t)8, maxN.pitch - minN.pitch);
				float scale = sizeContents.y / (float) numNotes;
				std::vector<const note_t*> notesClipped;
				for (const note_t& note : notes.m_list) {
					tick_t noteTime = note.time;
					if (noteTime >= clipLen) {
						notesClipped.push_back(&note);
						continue;
					}
					if (noteTime < 0) {
						notesClipped.push_back(&note);
						continue;
					}
					float objPosNote = noteTime /(float) TICKS_BAR;
		//			assert(objPosNote >= 0 && objPosNote < numBars);
					float objLenNote = note.len /(float) TICKS_BAR;
		//			assert(objPosNote+objLenNote >= 0);
					float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
					float nx = max(0.0f, objPosNote * barSize);
					float nw = min(objLenNote * barSize, sizeContents.x-nx);
					float nh = scale;
					float insetx = calcInset(1, nw);
					float insety = calcInset(1, nh);
					nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
				}
				nvgFillColor(vg, rgbToNvg(rgbNote));
				nvgFill(vg);
				if (!notesClipped.empty()) {
					nvgBeginPath(vg);
					for (const note_t* noteClipped : notesClipped) {
						const note_t& note = *noteClipped;
						tick_t noteTime = note.time;
			//			assert(objPosNote >= 0 && objPosNote < numBars);
			//			assert(objPosNote+objLenNote >= 0);

						float objPosNote = noteTime /(float) TICKS_BAR;
						float objLenNote = note.len /(float) TICKS_BAR;
						float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
						float nx = objPosNote * barSize;
						float nw = objLenNote * barSize;
						float nh = scale;
						float insetx = calcInset(1, nw);
						float insety = calcInset(1, nh);
						nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
					}
					nvgFillColor(vg, rgbToNvg(rgbNoteOverlap));
					nvgFill(vg);
				}
			}
		}
		nvgRestore(vg);
	}
	if (cl->loopEnabled) {
		tick_t posLoopIndicator = cl->getLoopBegin();
		nvgBeginPath(vg);
		while (posLoopIndicator < clipLen) {
			if (posLoopIndicator >= 0) {
				float objPos = posLoopIndicator /(float) TICKS_BAR;
				float nx = barSize*objPos;
				nvgMoveTo(vg, pos.x+nx, pos.y);
				nvgLineTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE/4);
				nvgMoveTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE*3/4);
				nvgLineTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE);
			}
			posLoopIndicator += cl->loopLen;
		}
		nvgStrokeColor(vg, GUI_COLOR(G_S2));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
	}
}

void gui_track::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}

void gui_clip::trackViewDragBegin(guitrack_editor* view, MouseEvent& evt) {
	view->dragSelectionBegin(this, evt);
}
void gui_clip::trackViewDragMove(guitrack_editor* view, MouseEvent& evt) {
	view->dragSelectionMove(this, evt);
}
void gui_clip::trackViewDragRelease(guitrack_editor* view, MouseEvent& evt) {
	view->dragSelectionRelease(this, evt);
	//!CLIP COULD BE DELETED AT THIS POINT
}

void gui_track::updateVisibleTrackContents(scaled_grid& grid) {
	track_plugins_t* data=this->m_track->audio;
	if (data) {
		automatable_t* ctr = data->selectedAutomationCtr;
		int32_t idx = data->selectedAutomationParam;
		automation.setData(ctr, idx);
	}
	else {

		automation.setData(NULL, -1);
	}
	automation.updateVisibleTrackContents(grid);
	for (clip_t* clip : midi.clips) {
		if (!clip->gClip) {
			clip->gClip = new gui_clip(clip, m_track);
			add(clip->gClip);
		}
		clip->gClip->updatePosition(grid, size);
	}
}
gui_track* createTrackGui(track_t* t, scaled_grid& grid) {
	return new gui_track(t, grid);
}
