#include "cliprenderer.h"
#include "../host/vst_host.h"
#include <glm/glm.hpp>
#include <glm/vec2.hpp>
using glm::vec2;
using glm::ivec2;

bool getClipPosition(scaled_grid& grid, const ivec2& trackSize, const clip_t* cl, ivec2& pos, ivec2& size, tick_t offset) {
	tick_t tickBegin = cl->time + offset;
	tick_t tickEnd = cl->time + offset + cl->getLen();
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
audioclip_texture_t makeWaveformFromClip(project_t& project, scaled_grid& grid, ivec2& trackSize, clip_t* m_clip, ivec2& pos, ivec2& size, ivec2& posClipped, ivec2& sizeClipped) {


	samplerate_t sr = vsthost::getInstance()->lSampleRate; //TODO: store in project_t
	double lenSamples = tickToSamplePrecise(m_clip->getLen(), project.tempo100, sr);
	double samplesPerPx = lenSamples/size.x;

	int32_t pxBegin = posClipped.x;
	int32_t pxEnd = posClipped.x + sizeClipped.x;
	double tickBegin = grid.screenToTickD(pos.x);
	double tickBeginOffset = grid.screenToTickD(pxBegin);
	double tickEnd = grid.screenToTickD(pxEnd);
//					tickBeginOffset += m_clip->offsetStart;
//					tickEnd += m_clip->offsetStart;

	double sampleBegin = tickToSamplePrecise(tickBegin, project.tempo100, sr);
	double sampleStartOffset = tickToSamplePrecise(tickBeginOffset, project.tempo100, sr);
	double sampleEnd = tickToSamplePrecise(tickEnd, project.tempo100, sr);
	sampleStartOffset += m_clip->offsetSamples;
	sampleEnd += m_clip->offsetSamples;
	ivec2 startOffset = posClipped - pos;
	audioclip_texture_t w;
	w.quality=4;
	if (samplesPerPx >= 256) {
		w.quality *= 2;
		w.scale = 2;
	}
	constexpr float MAX_RES = 512;
	w.scaleX = 1.0f;
	if (samplesPerPx > MAX_RES) {
		w.scaleX = MAX_RES/samplesPerPx;
		samplesPerPx = MAX_RES;
	}
//			if (samplesPerPx >= 256) {
//				waveform.quality *= 2;
//				waveform.scale = 2;
//			}
//					my_printf("sampleBegin %f\n", sampleBegin);
//					my_printf("sampleStartOffset %f\n", sampleStartOffset);
//					my_printf("sampleEnd %f\n", sampleEnd);
//					my_printf("m_clip->offsetStart %d\n", m_clip->offsetStart);
//					my_printf("m_clip->offsetSamples %d\n", m_clip->offsetSamples);
//	makeOrUpdateWaveform(&w, posClipped, startOffset, sizeClipped, sampleBegin, sampleStartOffset, sampleEnd, samplesPerPx, grid.zoom);
	w.pos = pos;
	w.startOffset = startOffset;
	w.size = size;
	w.sampleBegin = sampleBegin;
	w.sampleBeginOffset = sampleStartOffset;
	w.sampleEnd = sampleEnd;
	w.samplesPerPx = samplesPerPx;
	w.linewidth = 1.50f+min(0.75, max(0.0, grid.zoom*32.0));
	double scale = 0.005;
//	if (gridZoom < 0.03) {

		w.method = SampleMethod::sample_straight;
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
//		assert(w->sampleEnd>w->sampleBegin);
//		assert(w->sampleEnd>w->sampleBeginOffset);
	my_printf("waveform[height:%d,zoom:%f,q:%d,w:%f,smp/px:%f,scale:%d]\n", w.size.y, grid.zoom, w.quality, w.linewidth, w.samplesPerPx, w.scale);


	return w;

}

void renderAudioClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, const gui_waveform_texture_ref* guiaudioclip, ivec2 pos, ivec2 size) {
	if (cl->getLen() <= 0) {
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
//		setFont(vg, (int) (HEIGHT_CLIP_TITLE * 0.95), rgbaToNvg(-1), G_TITLE_ALIGN);
//		String text = StringFormat("%d", pos.x);
//		renderText(vg, pos.x + INSET_TITLE, pos.y + HEIGHT_CLIP_TITLE + 4, size.x-INSET_TITLE*3, StringAsCStr(text));

	}

	ivec2 posContents = ivec2(pos.x, pos.y+HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
	ivec2 sizeContents = ivec2(size.x, size.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);
	tick_t clipLen = cl->getLen();
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
float noteToScreen(float note, float scale, float offset, float sizeY) {
	float offsetKey = note * scale;
	float rel = offsetKey - offset;
	return (sizeY) - rel;
}
void renderMidiClip(NVGcontext* vg, const track_t* tr, const clip_t* cl, ivec2 pos, ivec2 size) {
	if (cl->getLen() <= 0) {
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

	tick_t clipLen = cl->getLen();
	float numBars = clipLen / (float) TICKS_BAR;
	float barSize = sizeContents.x / (float) numBars;
	if (sizeContents.x > 0 && sizeContents.y > 0) {
		nvgSave(vg);
		nvgTranslate(vg, posContents.x, posContents.y);

		clip_notes_t& notesView = cl->getNoteViewRender();
		clip_notes_t& notesPlay = cl->getNoteViewPlayback();
	//	clip_notes_t notesPlay;
	//	cl->getNotesView(0, cl->len, notesPlay, true);
		int32_t rgbNote = 0xffffff;
		int32_t rgbNoteOverlap = 0x0000ff;
		int32_t rgbNoteMuted = 0x121212;
		clip_notes_t& notes = notesView;
		if (!notes.empty()) {
			note_t minN = notesView.minNote;
			note_t maxN = notesView.maxNote;
			int32_t numNotes = max((int32_t)8, maxN.pitch - minN.pitch);
			float scale = sizeContents.y / (float) numNotes;
			std::vector<const note_t*> notesClipped;
			std::vector<const note_t*> notesMuted;
			int begin = 0;
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
				if (!note.enabled) {
					notesMuted.push_back(&note);
					continue;
				}
				if (!begin) {

					nvgBeginPath(vg);
					begin++;
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
			if (begin) {
				nvgFillColor(vg, rgbToNvg(rgbNote));
				nvgFill(vg);
			}

			for (int j = 0; j < 2; j++) {
				auto& list = j == 0 ? notesClipped : notesMuted;
				if (!list.empty()) {
					nvgBeginPath(vg);
					for (const note_t* noteClipped : list) {
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
					nvgFillColor(vg, rgbToNvg(j == 0 ? rgbNoteOverlap : rgbNoteMuted));
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
