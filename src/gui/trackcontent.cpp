
#include <glm/vec2.hpp>
#include <glm/geometric.hpp>


#include "track.h"
#include "trackautomation.h"
#include "track_impl.h"
#include "../host/vst_plugin.h"

#include "event.h"
#include "button.h"
#include "dropdown.h"
#include "guicontextmenu.h"
#include "trackctr.h"
#include "trackcontent.h"
#include "audiocache.h"
#include "drawwaveform.h"
#include "samplerate.h"
#include "../host/vst_host.h"


#include "leak_detect.h"

using glm::vec2;
using glm::ivec2;

float noteToScreen(float note, float scale, float offset, float sizeY) {
	float offsetKey = note * scale;
	float rel = offsetKey - offset;
	return (sizeY) - rel;
}
void gui_midi_clip::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}
void gui_audio_clip::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_clip(this->m_clip), evt.mousepos);
}

void renderMidiClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, ivec2 pos, ivec2 size) {
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
void makeOrUpdateWaveform(audioclip_texture_t* w, ivec2 pos, ivec2 startOffset, ivec2 size, double sampleBegin,  double sampleBeginOffset, double sampleEnd, double res, double gridZoom) {
	w->pos = pos;
	w->startOffset = startOffset;
	w->size = size;
	w->sampleBegin = sampleBegin;
	w->sampleBeginOffset = sampleBeginOffset;
	w->sampleEnd = sampleEnd;
	w->samplesPerPx = res;
	w->linewidth = 1.50f+min(0.75, max(0.0, gridZoom*32.0));
	double scale = 0.005;
//	if (gridZoom < 0.03) {

		w->method = SampleMethod::sample_straight;
		scale = 0.00005;
//	} else {
////
//		w->method = SampleMethod::sample_minmax;
//	}
//	int qu = 4;
//	w->quality = qu;

//	double d = gridZoom;
//	while (d > scale && qu < 16) {
//		d /= 2.0;
//		qu*=2;
//	}
	my_printf("waveform[zoom:%f,q:%d,w:%f,smp/px:%f,scale:%d]\n", gridZoom, w->quality, w->linewidth, w->samplesPerPx, w->scale);
}
void gui_audio_clip::releaseRendered() {
	waveformrender::getInstance()->release(this->fbId);
	this->fbId = -1;
	this->rendered = false;
}
void gui_audio_clip::updatePosition(project_t& project, scaled_grid& grid, ivec2& trackSize) {
	size = this->parent->size;
	culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);
	cachedaudio_t* audio = audiocache::getInstance()->get(m_clip->audio.id);
	if (culled || !audio) {
		releaseRendered();
	}
//test clipping
//	ivec2 prevSize = size;
//	ivec4 clippedP = ivec4(pos, size);
//	this->parent->scissorClip(clippedP);
//	pos.x = clippedP.x;
//	pos.y = clippedP.y;
//	size.x = clippedP.z;
//	size.y = clippedP.w;
//	if (size != prevSize) {
//		my_printf("%d %d -> %d %d\n", prevSize.x, prevSize.y, size.x, size.y);
//	}
	if (!culled) {
		assert(size.x > 0);
		if (audio) {
			samplerate_t sr = vsthost::getInstance()->lSampleRate; //TODO: store in project_t
			double lenSamples = tickToSamplePrecise(m_clip->len, project.tempo100, sr);
			double samplesPerPx = lenSamples/size.x;

			ivec2 posClipped = pos;
			ivec2 sizeClipped = ivec2(size.x, size.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);
			this->parent->scissorClip(posClipped, sizeClipped);
			int32_t pxBegin = posClipped.x;
			int32_t pxEnd = posClipped.x + sizeClipped.x;
			double tickBegin = grid.screenToTickD(pos.x);
			double tickBeginOffset = grid.screenToTickD(pxBegin);
			double tickEnd = grid.screenToTickD(pxEnd);
//			tickBegin += m_clip->offsetStart;
			tickBeginOffset += m_clip->offsetStart;
			tickEnd += m_clip->offsetStart;
			double sampleBegin = tickToSamplePrecise(tickBegin, project.tempo100, sr);
			double sampleStartOffset = tickToSamplePrecise(tickBeginOffset, project.tempo100, sr);
			double sampleEnd = tickToSamplePrecise(tickEnd, project.tempo100, sr);
			ivec2 startOffset = posClipped - pos;
			audioclip_texture_t waveform;
			waveform.quality=4;
			if (samplesPerPx >= 256) {
				waveform.quality *= 2;
				waveform.scale = 2;
			}
			constexpr float MAX_RES = 512;
			waveform.scaleX = 1.0f;
			if (samplesPerPx > MAX_RES) {
				waveform.scaleX = MAX_RES/samplesPerPx;
				samplesPerPx = MAX_RES;
			}
//			if (samplesPerPx >= 256) {
//				waveform.quality *= 2;
//				waveform.scale = 2;
//			}
			makeOrUpdateWaveform(&waveform, posClipped, startOffset, sizeClipped, sampleBegin, sampleStartOffset, sampleEnd, samplesPerPx, grid.zoom);
			if (waveform != this->waveform) {
				releaseRendered();
				this->waveform = waveform;
			}
		}
	}
}
void gui_audio_clip::prerender(NVGcontext* vg) {
//	my_printf("gui_audio_clip::prerender\n",0);
	if (!culled && !rendered) {
		cachedaudio_t* audio = audiocache::getInstance()->get(m_clip->audio.id);
		if (audio) {
			int ret = waveformrender::getInstance()->render(vg, audio, &this->waveform, 1);
			this->fbId = ret;
			this->rendered = true;
		}
	}
}
void renderAudioClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, gui_audio_clip* guiaudioclip, ivec2 pos, ivec2 size) {
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
		setFont(vg, (int) (HEIGHT_CLIP_TITLE * 0.95), rgbaToNvg(-1), G_TITLE_ALIGN);
		String text = StringFormat("%d", pos.x);
		renderText(vg, pos.x + INSET_TITLE, pos.y + HEIGHT_CLIP_TITLE + 4, size.x-INSET_TITLE*3, StringAsCStr(text));

	}

	ivec2 posContents = ivec2(pos.x, pos.y+HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
	ivec2 sizeContents = ivec2(size.x, size.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);
	tick_t clipLen = cl->len;
	float numBars = clipLen / (float) TICKS_BAR;
	float barSize = sizeContents.x / (float) numBars;
	if (sizeContents.x > 0 && sizeContents.y > 0 && guiaudioclip->rendered) {
		nvgSave(vg);
		nvgTranslate(vg, posContents.x, posContents.y);
		waveformrender::getInstance()->draw(vg, guiaudioclip->fbId, &guiaudioclip->waveform, sizeContents);

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

bool getClipPosition(scaled_grid& grid, const ivec2& trackSize, const clip_t* cl, ivec2& pos, ivec2& size, tick_t offset) {
	tick_t tickBegin = cl->time + offset;
	tick_t tickEnd = cl->time + offset + cl->len;
	grid.debug = true;
	double tickBeginX = grid.tickToScreenD(tickBegin);
	grid.debug = false;
	double tickEndX = grid.tickToScreenD(tickEnd);
	if (tickEndX < -4 || tickBeginX > trackSize.x + 4) {
		return false;
	}
	double width = tickEndX - tickBeginX;
	assert(FitsTypeRange<int32_t>(tickBeginX));
	assert(FitsTypeRange<int32_t>(tickEndX));
	int32_t tickBeginPx = (int32_t) round(tickBeginX);
	int32_t widthPx = (int32_t) round(width);
	pos = ivec2(tickBeginPx, INSET_TRACK_CONTENT);
	size = ivec2(widthPx, size.y-INSET_TRACK_CONTENT*2);
	if (size.x <= 0 || size.y <= 0) {
		my_printf("culled because of size!\n",0);
	}
	return size.x > 0 && size.y > 0;
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

void gui_track::updateVisibleTrackContents(project_t& project, scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
	for (clip_t* clip : midi.clips) {
		if (!clip->gClip) {
			if (clip->clipType == CLIP_MIDI) {
				clip->gClip = new gui_midi_clip(m_track, clip);
			} else {
				clip->gClip = new gui_audio_clip(m_track, clip);
			}
			add(clip->gClip);
		}
		clip->gClip->updatePosition(project, grid, size);
	}
}

void gui_track_automationlane::updateVisibleTrackContents(scaled_grid& grid) {
	automation.setData();
	automation.updateVisibleTrackContents(grid);
}

gui_track_automationlane::gui_track_automationlane(track_t* _track, scaled_grid& _grid, automatable_t* _at, int32_t _param)
  : guictr_base(), m_track(_track), automation(_track, _grid, at, param, idx), at(_at), param(_param)
{
	padding = 0;
}
gui_track::gui_track(track_t* _track, scaled_grid& _grid)
  : guictr_base(), m_track(_track), midi(_track->getMidi()), automation(_track, _grid, m_track->audio->selectedAutomationCtr, m_track->audio->selectedAutomationParam, subtrackIdx)
{
	padding = 0;
}
gui_track* createTrackGui(track_t* t, scaled_grid& grid) {
	return new gui_track(t, grid);
}



class guictxtmenu_trackcontent : public guictxtmenu_base {
public:
	int32_t trackid;
	guictxtmenu_trackcontent(int32_t _trackid) {
		this->trackid = _trackid;
		this->size.x = 320;
		MainCtrl* ctrl = MainCtrl::get();
		track_t* tr = ctrl->getTrackId(this->trackid);
		auto newClip = new ctxtmenu_entry("Create clip", 20);
		add(newClip);
		add(new ctxtmenu_splitter());
		scaled_grid& grid = MainCtrl::get()->getGrid();
		auto adaptive = new ctxtmenu_time_select(grid, "Adaptive Grid", 0);
		adaptive->initAdaptive();
		add(adaptive);
		auto fixed = new ctxtmenu_time_select(grid, "Fixed Grid", 0);
		fixed->initFixed();
		add(fixed);
	}
	void clicked(int _id) {
		MainCtrl* ctrl = MainCtrl::get();
		scaled_grid& grid = ctrl->getGrid();
		if (_id == 20) {
			Cursor cursor = MainCtrl::get()->cursor.getLeftAligned();
			if (cursor.selRange) {
				track_t* tr = ctrl->getTrackId(this->trackid);
				if (tr && tr->type == TRACK_TYPE_MIDI) {
					clip_t* cl = new clip_t(CLIP_MIDI, StringFormat("%s Clip", StringAsCStr(tr->name)));
					cl->time = cursor.cursorPos;
					cl->len = cursor.selRange;
					cl->loopStart = 0;
					cl->loopLen = cl->len;
					tr->getMidi().addClipSort(cl);
				}
				if (tr && tr->type == TRACK_TYPE_AUDIO) {
					clip_t* cl = new clip_t(CLIP_AUDIO, StringFormat("%s", StringAsCStr(tr->name)));
					cl->time = cursor.cursorPos;
					cl->len = cursor.selRange;
					cl->loopStart = 0;
					cl->loopLen = cl->len;
					tr->getMidi().addClipSort(cl);
				}
			}
		}
		else if (_id == 110+9) { // OFF
			grid.grid_dens.enabled = false;
		} else if (_id >= 110) {
			grid.grid_dens.enabled = true;
			grid.grid_dens.fixedBars = _id - 110;
			grid.grid_dens.isfixed = true;
		} else {
			grid.grid_dens.enabled = true;
			grid.grid_dens.dynamicDensity = _id - 100;
			grid.grid_dens.isfixed = false;
		}
//		ctrl->updateVisibleTrackContents();
		MainCtrl::get()->updateGrid();

		MainCtrl::get()->closeContextMenu();
	}
};
void gui_track_automationlane::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}
void gui_track::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(this->m_track->idx), evt.mousepos);
}
void guitrack_editor::handleRightClick(MouseEvent& evt) {
	MainCtrl::get()->openContextMenu(new guictxtmenu_trackcontent(-1), evt.mousepos);
}
