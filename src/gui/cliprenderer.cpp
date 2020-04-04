#include "cliprenderer.h"
#include "math/seq_math.h"
#include "../host/vst_host.h"
#include "theme.h"
#include "gui.h"
#include "seq_time.h"
#include "project.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "drawwaveform.h"
#include "audiowaveform.h"
#include "trackcontent.h"

namespace GuiColor {
constant_t COL_CLIP_OUTLINE("COL_CLIP_OUTLINE", 0x0);

constant_t COL_CLIP_NOTE("COL_CLIP_NOTE", 0xFFFFFFFF);
constant_t COL_CLIP_NOTE_OVERLAP("COL_CLIP_NOTE_OVERLAP", 0xFF0000FF);
constant_t COL_CLIP_NOTE_MUTED("COL_CLIP_NOTE_MUTED", 0xFF121212);
}

struct noteview_cache_impl_t {
	bool valid = false;
	int64_t notesRendered=-1;
	ivec2 pos={-1,-1};
	ivec2 size={-1,-1};
	std::array<nvg_path_cache_storage_t*,4> arr;
	noteview_cache_impl_t() {
		std::for_each(arr.begin(), arr.end(), [](nvg_path_cache_storage_t*& ptr) {
			ptr = nullptr;
		});
	}
	~noteview_cache_impl_t() {
		reset();
	}
	void SaveFill(NVGcontext* vg, int n) {
		dbgassert(n < arr.size());
		dbgassert(arr[n] == nullptr);
		arr[n] = nullptr;
		nvgGetLastCacheResult(vg, &arr[n]);
		NVGCacheEntryInfo cacheEntryInfo;
		nvgCacheEntryInfo(NULL, arr[n], &cacheEntryInfo);
		nvg_path_cache_storage_t* entry = arr[n];
		dbgassert(entry);
		daw_tls::tlsinstance& tls = daw_tls::getTls();
		tls.renderClipCacheStats.sizeCacheAllocatedMemBytes+= cacheEntryInfo.allocationSizeBytes;
	}
	bool isCacheValid(int n) {
		return valid && n < arr.size() && arr[n] != nullptr;
	}
	void reset() {
		valid = false;
		std::for_each(arr.begin(), arr.end(), [](nvg_path_cache_storage_t*& ptr) {
			if (ptr) {
				NVGCacheEntryInfo cacheEntryInfo;
				nvgCacheEntryInfo(NULL, ptr, &cacheEntryInfo);
				daw_tls::tlsinstance& tls = daw_tls::getTls();
				tls.renderClipCacheStats.sizeCacheAllocatedMemBytes -= cacheEntryInfo.allocationSizeBytes;

				nvgReleaseCacheResult(ptr);
				ptr = nullptr;
			}
		});
	}
};
struct midi_clip_render_cache_t : public noteview_cache_impl_t {
public:
	midi_clip_render_cache_t() : noteview_cache_impl_t() {

	}
};

bool getClipPosition(scaled_grid& grid, const ivec2& scissorSize, const clip_t* cl, ivec2& pos, ivec2& size, tick_t offset) {
	tick_t tickBegin = cl->time + offset;
	tick_t tickEnd = cl->time + offset + cl->getLen();
	grid.debug = true;
	double tickBeginX = grid.tickToScreenD(tickBegin);
	grid.debug = false;
	double tickEndX = grid.tickToScreenD(tickEnd);
	if (tickEndX < -4 || tickBeginX > scissorSize.x + 4) {
		return false;
	}
	double width = tickEndX - tickBeginX;
	dbgassert(FitsTypeRange<int32_t>(tickBeginX));
	dbgassert(FitsTypeRange<int32_t>(tickEndX));
	int32_t tickBeginPx = (int32_t) round(tickBeginX);
	int32_t widthPx = (int32_t) round(width);
	pos = ivec2(tickBeginPx, INSET_TRACK_CONTENT);
	size = ivec2(widthPx, size.y-INSET_TRACK_CONTENT*2);
	return size.x > 0 && size.y > 0;
}
audioclip_texture_t makeWaveformFromClip(const project_globals_t& project, scaled_grid& grid,
		ivec2& trackSize, clip_t* m_clip, ivec2& pos, ivec2& size, ivec2& posClipped, ivec2& sizeClipped) {


	samplerate_t sr = vsthost::getInstance()->sampleFormat.sampleRate; //TODO: store in project_t
	double lenSamples = tickToSamplePrecise(m_clip->getLen(), project.tempo100, sr);
	double samplesPerPx = lenSamples/size.x;

	int32_t pxBegin = posClipped.x;
	int32_t pxEnd = posClipped.x + sizeClipped.x;
	double tickBegin = grid.screenToTickD(pos.x);
	double tickBeginOffset = grid.screenToTickD(pxBegin);
	double tickEnd = grid.screenToTickD(pxEnd);
	if (size.x == sizeClipped.x) {
		tickBeginOffset = m_clip->start();
		tickBegin = m_clip->start();
		tickEnd = m_clip->end();
	}
	int64_t sampleBegin = math::floorCast(tickToSamplePrecise(tickBegin, project.tempo100, sr));
	int64_t sampleStartOffset = math::floorCast(tickToSamplePrecise(tickBeginOffset, project.tempo100, sr));

	int64_t sampleEnd = math::floorCast(tickToSamplePrecise(tickEnd, project.tempo100, sr));
	sampleStartOffset += m_clip->offsetSamples;
	sampleEnd += m_clip->offsetSamples;
	ivec2 startOffset = posClipped - pos;
	audioclip_texture_t w;
	w.quality=4;

	double pxPerSample = 1.0/samplesPerPx;
//	if (samplesPerPx >= 256 && size.y * (w.scale*2) <= FBO_HEIGHT) {
//		w.quality *= 2;
//		w.scale *= 2;
//	}
//	if (samplesPerPx >= 512 && size.y * (w.scale*2) <= FBO_HEIGHT) {
//		w.quality *= 2;
//		w.scale *= 2;
//	}
//	if (samplesPerPx >= 1024 && size.y * (w.scale*2) <= FBO_HEIGHT) {
//		w.quality *= 2;
//		w.scale *= 2;
//	}
//	if (size.y * 4 <= FBO_HEIGHT) {
//		w.quality = 2;
//		w.scale = 4;
//	}
	constexpr float MAX_RES = 2048;
	w.scaleX = 1.0f;
	w.pos = pos;
//	w.startOffset = startOffset;
	w.size = ivec2(math::min(sizeClipped.x, FBO_WIDTH), math::min(size.y, FBO_HEIGHT));
	int64_t nSamples = sampleEnd-sampleStartOffset;
	if (nSamples * pxPerSample > FBO_WIDTH) {
		samplesPerPx = (nSamples / FBO_WIDTH);
	}
//	if (w.scale == 1) {
//		w.size.x = math::min(FBO_WIDTH, (int)std::round(fsizeX));
		if (samplesPerPx > MAX_RES && (nSamples / MAX_RES) <= FBO_WIDTH) {
			w.scaleX = MAX_RES/samplesPerPx;
			samplesPerPx = MAX_RES;
//			my_printf("w.scaleX %f\n", w.scaleX);
//			size.x *= 2;
		}
//		int n = 0;
//		double d = 2;
//		while (n++ < 5) {
//			w.scaleX /= d;
//			samplesPerPx /= d;
//		}
//	}
//	pxPerSample = 1.0/samplesPerPx;
//	double width = (sampleEnd-sampleStartOffset)*pxPerSample*w.scale;
//	dbgassert(width <= FBO_WIDTH);
	dbgassert(w.size.x <= FBO_WIDTH && w.size.y <= FBO_HEIGHT);
	dbgassert(w.size.x > 0);
	w.sampleBegin = sampleBegin;
	w.sampleBeginOffset = sampleStartOffset;
	w.sampleEnd = sampleEnd;
	w.samplesPerPx = samplesPerPx;
	w.linewidth = 1.5f;//+min(0.75, max(0.0, grid.zoom*32.0));
	w.method = SampleMethod::sample_straight;
	w.audioId = m_clip->audio.id;
	w.clipped = size.x != sizeClipped.x;
//	my_printf("waveform[height:%d,zoom:%f,q:%d,w:%f,smp/px:%f,scale:%d]\n", w.size.y, grid.zoom, w.quality, w.linewidth, w.samplesPerPx, w.scale);


	return w;

}

void renderAudioClip(NVGcontext* vg, const guitheme_t* theme, const track_t* tr, const clip_t* cl, const gui_waveform_texture_ref* waveformRef, ivec2 pos, ivec2 size, ivec2 posClipped, ivec2 sizeClipped) {
	if (cl->getLen() <= 0) {
		return;
	}
	NVGcolor color = rgbToNvg(cl->rgb);
	nvgBeginPath(vg);
	nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
	nvgFillColor(vg, color);
	nvgFill(vg);
	nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CLIP_OUTLINE));
	nvgStrokeWidth(vg, 1.f);
	nvgStroke(vg);

	if (cl->name.length()) {
		UTIL_setFont(vg, theme, (int) (HEIGHT_CLIP_TITLE * 0.95), getContrastFontColor(cl->rgb), G_TITLE_ALIGN);
		renderText(vg, pos.x + INSET_TITLE, pos.y + HEIGHT_CLIP_TITLE / 2, size.x-INSET_TITLE*3, StringAsCStr(cl->name));
//		setFont(vg, (int) (HEIGHT_CLIP_TITLE * 0.95), rgbaToNvg(-1), G_TITLE_ALIGN);
//		String text = StringFormat("%d", pos.x);
//		renderText(vg, pos.x + INSET_TITLE, pos.y + HEIGHT_CLIP_TITLE + 4, size.x-INSET_TITLE*3, StringAsCStr(text));

	}

	ivec2 posContents = ivec2(posClipped.x, pos.y+HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
//	ivec2 sizeContents = ivec2(sizeClipped.x, sizeClipped.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);
	tick_t clipLen = cl->getLen();
	float numBars = clipLen / (float) TICKS_BAR;
	float barSize = size.x / (float) numBars;
	if (sizeClipped.x > 0 && sizeClipped.y > 0 && waveformRef->rendered) {
		nvgSave(vg);
		nvgTranslate(vg, posContents.x, posContents.y);

		waveformrender::getInstance()->draw(vg, waveformRef, sizeClipped);

		nvgRestore(vg);
	}
	if (cl->loopEnabled && cl->loopLen > 0) {
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
		nvgStrokeColor(vg, theme->getFrameColorBase());
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
	}
}
float noteToScreen(float note, float scale, float offset, float sizeY) {
	float offsetKey = note * scale;
	float rel = offsetKey - offset;
	return (sizeY) - rel;
}


noteview_render_t::~noteview_render_t() {
	if (data) {
		delete data;
	}
}

gui_midi_clip::gui_midi_clip(track_gui_entry_t* _track, clip_t* _clip)
	: gui_clip(_track, _clip), impl(new midi_clip_render_cache_t)  {

}
gui_midi_clip::~gui_midi_clip() {
	delete impl;
}
void gui_midi_clip::updatePosition(project_globals_t& project, scaled_grid& grid, ivec2& trackSize) {
	size = this->parent->size;
	culled = !getClipPosition(grid, trackSize, m_clip, pos, size, 0);
	//bool resetCache = false;
	//if (!culled && impl->valid) {
	//	ivec2 posContents = ivec2(pos.x, pos.y + HEIGHT_CLIP_TITLE + INSET_CLIP_CONTENT);
	//	ivec2 sizeContents = ivec2(size.x, size.y - HEIGHT_CLIP_TITLE - INSET_CLIP_CONTENT * 2);
	//	const bool useCaching = true;// daw_tls::getTls().renderStats.enableCache;
	//	noteview_render_t& notesView = m_clip->getNoteViewRender();
	//	bool cacheValid = notesView.reqRevision == notesView.curRevision;
	//	cacheValid &= impl->cache1 != nullptr;
	//	cacheValid &= impl->pos == posContents;
	//	cacheValid &= impl->size == sizeContents;
	//	cacheValid &= useCaching;
	//	if (!cacheValid) {
	//		resetCache;
	//	}
	//} else if (culled && impl->valid) {
	//	resetCache = true;
	//}
	//if (resetCache) {
		impl->reset();
	//}
}
void gui_midi_clip::prerender(NVGcontext* vg) {
	gui_clip::prerender(vg);
}
void gui_midi_clip::updateClipRenderCache(NVGcontext* vg) {
	if (culled) {
		return;
	}

	clip_t* const cl = m_clip;
	track_t* const tr = m_track;
	if (cl->getLen() <= 0) {
		return;
	}

	ivec2 posContents = ivec2(pos.x, pos.y+HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
	ivec2 sizeContents = ivec2(size.x, size.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);
	ivec2 clipPosScreen = toScreenSpace(ivec2(0, 0));

	tick_t clipLen = cl->getLen();
	float numBars = clipLen / (float) TICKS_BAR;
	float barSize = sizeContents.x / (float) numBars;
	int64_t notesRendered=0;
	noteview_render_t& notesView = cl->getNoteViewRender();
	bool cacheValid = notesView.reqRevision == notesView.curRevision;
	cacheValid &= impl->valid;
	cacheValid &= impl->pos == posContents;
	cacheValid &= impl->size == sizeContents;
	if (!cacheValid) {
		notesView.curRevision = -1;
		impl->reset();


		//TODO: move this up in hierachy so its called once
	//	NVGcolor col = getTheme()->getColor(GuiColor::COL_CLEAR_COLOR);
	//	glClearColor(col.r, col.g, col.b, col.a);
	//	glClear(GL_COLOR_BUFFER_BIT);
	//	static int test = 0;
		NVGcolor rgbNote = theme->getColor(GuiColor::COL_CLIP_NOTE);
		NVGcolor rgbNoteOverlap = theme->getColor(GuiColor::COL_CLIP_NOTE_OVERLAP);
		NVGcolor rgbNoteMuted = theme->getColor(GuiColor::COL_CLIP_NOTE_MUTED);

		nvgSave(vg);
		nvgTranslate(vg, clipPosScreen.x, clipPosScreen.y);
		nvgSave(vg);
		nvgTranslate(vg, 0, HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
		if (sizeContents.x > 0 && sizeContents.y > 0) {
			clip_notes_t& notes = notesView;
			if (!notes.empty()) {
				note_t minN = notesView.minNote;
				note_t maxN = notesView.maxNote;
				int32_t numNotes = math::max((int32_t)8, maxN.pitch - minN.pitch);
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
					if (!note.isEnabled()) {
						notesMuted.push_back(&note);
						continue;
					}
					if (!begin) {

	//					void nvgCachePath(NVGcontext* ctx, int enabled);
						nvgBeginPath(vg);
						begin++;
					}
					float objPosNote = noteTime /(float) TICKS_BAR;
		//			dbgassert(objPosNote >= 0 && objPosNote < numBars);
					float objLenNote = note.len /(float) TICKS_BAR;
		//			dbgassert(objPosNote+objLenNote >= 0);
					float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
					float nx = math::max(0.0f, objPosNote * barSize);
					float nw = math::min(objLenNote * barSize, sizeContents.x-nx);
					float nh = scale;
					float insetx = calcInset(1, nw);
					float insety = calcInset(1, nh);
					nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
					notesRendered++;
	//				if (notesRendered % 1000 == 0) {
	//					nvgFillColor(vg, rgbNote);
	//					nvgFill(vg);
	//					begin = 0;
	//				}
				}
				if (begin) {
					nvgFillColor(vg, rgbNote);
					nvgFill(vg);
					impl->SaveFill(vg, 0);
				}

				for (int j = 0; j < 2; j++) {
					auto& list = j == 0 ? notesClipped : notesMuted;
					if (!list.empty()) {
						nvgBeginPath(vg);
						for (const note_t* noteClipped : list) {
							const note_t& note = *noteClipped;
							tick_t noteTime = note.time;
				//			dbgassert(objPosNote >= 0 && objPosNote < numBars);
				//			dbgassert(objPosNote+objLenNote >= 0);

							float objPosNote = noteTime /(float) TICKS_BAR;
							float objLenNote = note.len /(float) TICKS_BAR;
							float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
							float nx = objPosNote * barSize;
							float nw = objLenNote * barSize;
							float nh = scale;
							float insetx = calcInset(1, nw);
							float insety = calcInset(1, nh);
							nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
							notesRendered++;
						}
						nvgFillColor(vg, j == 0 ? rgbNoteOverlap : rgbNoteMuted);
						nvgFill(vg);
						if (j == 0) {
//							nvgGetLastCacheResult(vg, &impl->cache2);
							impl->SaveFill(vg, 1);
						} else {
//							nvgGetLastCacheResult(vg, &impl->cache3);
							impl->SaveFill(vg, 2);
						}

					}
				}
			}
		}
		nvgRestore(vg);

		if (cl->isLoopEnabled()) {
			tick_t posLoopIndicator = cl->getLoopBegin();
			int n = 0;
			while (posLoopIndicator < clipLen) {
				if (posLoopIndicator >= 0) {
					float objPos = posLoopIndicator /(float) TICKS_BAR;
					float nx = barSize*objPos;
					if (n == 0) {
						nvgBeginPath(vg);
					}
					nvgMoveTo(vg, nx, 0);
					nvgLineTo(vg, nx, 0+HEIGHT_CLIP_TITLE/4);
					nvgMoveTo(vg, nx, 0+HEIGHT_CLIP_TITLE*3/4);
					nvgLineTo(vg, nx, 0+HEIGHT_CLIP_TITLE);
					n++;
				}
				posLoopIndicator += cl->loopLen;
			}
			if (n) {
				nvgStrokeColor(vg, theme->getFrameColorBase());
				nvgStrokeWidth(vg, 1.f);
				nvgStroke(vg);
//				nvgGetLastCacheResult(vg, &impl->cache4);
				impl->SaveFill(vg, 3);
			}
		}
		nvgRestore(vg);
		impl->valid = true;
		impl->pos = posContents;
		impl->size = sizeContents;
		impl->notesRendered = notesRendered;
		notesView.curRevision = notesView.reqRevision;


	}
}

void gui_midi_clip::render(NVGcontext* vg) {
	if (!culled) {
		clip_t* const cl = m_clip;
		track_t* const tr = m_track;
		if (cl->getLen() <= 0) {
			return;
		}
		NVGcolor color = rgbToNvg(cl->rgb);
		if (!cl->enabled) {
			color = rgbToNvg(0x333333);
		}
		nvgBeginPath(vg);
		nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
		nvgFillColor(vg, color);
		nvgFill(vg);
		nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CLIP_OUTLINE));
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
		if (cl->name.length() && size.x - INSET_TITLE*3 >= 5) {
			UTIL_setFont(vg, theme, (int) (HEIGHT_CLIP_TITLE * 0.95), getContrastFontColor(cl->rgb), G_TITLE_ALIGN);
			renderText(vg, pos.x + INSET_TITLE, pos.y + HEIGHT_CLIP_TITLE / 2, size.x-INSET_TITLE*3, StringAsCStr(cl->name));
//			return;
		}
		ivec2 posContents = ivec2(pos.x, pos.y+HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
		ivec2 sizeContents = ivec2(size.x, size.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);

		tick_t clipLen = cl->getLen();
		float numBars = clipLen / (float) TICKS_BAR;
		float barSize = sizeContents.x / (float) numBars;
		int64_t notesRendered=0;
		const bool useCaching = true;
		noteview_render_t& notesView = cl->getNoteViewRender();
		bool cacheValid = notesView.reqRevision == notesView.curRevision;
		cacheValid &= impl->valid;
		cacheValid &= impl->pos == posContents;
		cacheValid &= impl->size == sizeContents;
		cacheValid &= useCaching;

		NVGcolor rgbNote = theme->getColor(GuiColor::COL_CLIP_NOTE);
		NVGcolor rgbNoteOverlap = theme->getColor(GuiColor::COL_CLIP_NOTE_OVERLAP);
		NVGcolor rgbNoteMuted = theme->getColor(GuiColor::COL_CLIP_NOTE_MUTED);
		if (cacheValid) {
			nvgSave(vg);
			nvgTranslate(vg, pos.x, pos.y);
			nvgSave(vg);
			nvgTranslate(vg, 0, HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
//			nvgTranslate(vg, posContents.x, posContents.y);
			if (impl->isCacheValid(0)) {
				nvgFillFromCache(vg, impl->arr[0]);
			}
			if (impl->isCacheValid(1)) {
				nvgFillFromCache(vg, impl->arr[1]);
			}
			if (impl->isCacheValid(2)) {
				nvgFillFromCache(vg, impl->arr[2]);
			}
			notesRendered += impl->notesRendered;
			nvgRestore(vg);
			if (cl->isLoopEnabled()) {
				if (impl->isCacheValid(3)) {
					nvgFillFromCache(vg, impl->arr[3]);
				}
			}
			nvgRestore(vg);
		}
//
//		if (useCaching && impl) {
//			impl->pos = posContents;
//			impl->size = sizeContents;
//			impl->notesRendered = notesRendered;
//			notesView.curRevision = notesView.reqRevision;
//		}
		daw_tls::getTls().renderStats.clipsRendered++;
		daw_tls::getTls().renderStats.notesRendered+=notesRendered;

	}
}
void renderMidiClip(NVGcontext* vg, const guitheme_t* theme, const track_gui_entry_t* const entry, const clip_t* cl, ivec2 pos, ivec2 size) {
	if (cl->getLen() <= 0) {
		return;
	}
	NVGcolor color = rgbToNvg(cl->rgb);
	if (!cl->enabled) {
		color = rgbToNvg(0x333333);
	}
	nvgBeginPath(vg);
	nvgRect(vg, pos.x, pos.y, size.x, HEIGHT_CLIP_TITLE);
	nvgFillColor(vg, color);
	nvgFill(vg);
	nvgStrokeColor(vg, theme->getColor(GuiColor::COL_CLIP_OUTLINE));
	nvgStrokeWidth(vg, 1.f);
	nvgStroke(vg);
	if (cl->name.length() && size.x-INSET_TITLE*3 >= 5) {
		UTIL_setFont(vg, theme, (int) (HEIGHT_CLIP_TITLE * 0.95), getContrastFontColor(cl->rgb), G_TITLE_ALIGN);
		renderText(vg, pos.x + INSET_TITLE, pos.y + HEIGHT_CLIP_TITLE / 2, size.x-INSET_TITLE*3, StringAsCStr(cl->name));
	}
	ivec2 posContents = ivec2(pos.x, pos.y+HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
	ivec2 sizeContents = ivec2(size.x, size.y-HEIGHT_CLIP_TITLE-INSET_CLIP_CONTENT*2);

	tick_t clipLen = cl->getLen();
	float numBars = clipLen / (float) TICKS_BAR;
	float barSize = sizeContents.x / (float) numBars;
	int64_t notesRendered=0;
	const bool useCaching = daw_tls::getTls().renderStats.enableCache;
	noteview_render_t& notesView = cl->getNoteViewRender();
	bool cacheValid = notesView.reqRevision == notesView.curRevision;
	cacheValid &= notesView.data != nullptr && notesView.data->valid;
	cacheValid &= notesView.data != nullptr && notesView.data->pos == posContents;
	cacheValid &= notesView.data != nullptr && notesView.data->size == sizeContents;
	cacheValid &= useCaching;
	if (!cacheValid) {
		notesView.curRevision = -1;
		if (notesView.data != nullptr) {
			notesView.data->reset();
		} else {
			notesView.data = new noteview_cache_impl_t{};
		}

	}

	NVGcolor rgbNote = theme->getColor(GuiColor::COL_CLIP_NOTE);
	NVGcolor rgbNoteOverlap = theme->getColor(GuiColor::COL_CLIP_NOTE_OVERLAP);
	NVGcolor rgbNoteMuted = theme->getColor(GuiColor::COL_CLIP_NOTE_MUTED);
	if (cacheValid) {
		nvgSave(vg);
		nvgTranslate(vg, pos.x, pos.y);
		nvgSave(vg);
		nvgTranslate(vg, 0, HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT);
		if (notesView.data->isCacheValid(0)) {
			nvgFillFromCache(vg, notesView.data->arr[0]);
		}
		if (notesView.data->isCacheValid(1)) {
			nvgFillFromCache(vg, notesView.data->arr[1]);
		}
		if (notesView.data->isCacheValid(2)) {
			nvgFillFromCache(vg, notesView.data->arr[2]);
		}
		nvgRestore(vg);
		notesRendered += notesView.data->notesRendered;
		if (notesView.data->isCacheValid(3)) {
			nvgFillFromCache(vg, notesView.data->arr[3]);
		}
		nvgRestore(vg);
	} else {
		nvgCachePath(vg, useCaching);
		nvgSave(vg);
		nvgTranslate(vg, posContents.x, posContents.y);
		if (sizeContents.x > 0 && sizeContents.y > 0) {
			clip_notes_t& notes = notesView;
			if (!notes.empty()) {
				note_t minN = notesView.minNote;
				note_t maxN = notesView.maxNote;
				int32_t numNotes = math::max((int32_t)8, maxN.pitch - minN.pitch);
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
					if (!note.isEnabled()) {
						notesMuted.push_back(&note);
						continue;
					}
					if (!begin) {

	//					void nvgCachePath(NVGcontext* ctx, int enabled);
						nvgBeginPath(vg);
						begin++;
					}
					float objPosNote = noteTime /(float) TICKS_BAR;
		//			dbgassert(objPosNote >= 0 && objPosNote < numBars);
					float objLenNote = note.len /(float) TICKS_BAR;
		//			dbgassert(objPosNote+objLenNote >= 0);
					float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
					float nx = math::max(0.0f, objPosNote * barSize);
					float nw = math::min(objLenNote * barSize, sizeContents.x-nx);
					float nh = scale;
					float insetx = calcInset(1, nw);
					float insety = calcInset(1, nh);
					nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
					notesRendered++;
	//				if (notesRendered % 1000 == 0) {
	//					nvgFillColor(vg, rgbNote);
	//					nvgFill(vg);
	//					begin = 0;
	//				}
				}
				if (begin) {
					nvgFillColor(vg, rgbNote);
					nvgFill(vg);
					if (useCaching) {
						notesView.data->SaveFill(vg, 0);
					}
				}

				for (int j = 0; j < 2; j++) {
					auto& list = j == 0 ? notesClipped : notesMuted;
					if (!list.empty()) {
						nvgBeginPath(vg);
						for (const note_t* noteClipped : list) {
							const note_t& note = *noteClipped;
							tick_t noteTime = note.time;
				//			dbgassert(objPosNote >= 0 && objPosNote < numBars);
				//			dbgassert(objPosNote+objLenNote >= 0);

							float objPosNote = noteTime /(float) TICKS_BAR;
							float objLenNote = note.len /(float) TICKS_BAR;
							float ny = noteToScreen(note.pitch-minN.pitch, scale, 0, sizeContents.y);
							float nx = objPosNote * barSize;
							float nw = objLenNote * barSize;
							float nh = scale;
							float insetx = calcInset(1, nw);
							float insety = calcInset(1, nh);
							nvgRect(vg, nx+insetx, ny+insety, nw-insetx*2, nh-insety*2);
							notesRendered++;
						}
						nvgFillColor(vg, j == 0 ? rgbNoteOverlap : rgbNoteMuted);
						nvgFill(vg);
						if (useCaching) {
							if (j == 0) {
								notesView.data->SaveFill(vg, 1);
							} else {
								notesView.data->SaveFill(vg, 2);
							}
						}

					}
				}
			}
		}
		nvgRestore(vg);

		if (cl->isLoopEnabled()) {
			tick_t posLoopIndicator = cl->getLoopBegin();
			int n = 0;
			while (posLoopIndicator < clipLen) {
				if (posLoopIndicator >= 0) {
					float objPos = posLoopIndicator /(float) TICKS_BAR;
					float nx = barSize*objPos;
					if (n == 0) {
						nvgBeginPath(vg);
					}
					nvgMoveTo(vg, pos.x+nx, pos.y);
					nvgLineTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE/4);
					nvgMoveTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE*3/4);
					nvgLineTo(vg, pos.x+nx, pos.y+HEIGHT_CLIP_TITLE);
					n++;
				}
				posLoopIndicator += cl->loopLen;
			}
			if (n) {
				nvgStrokeColor(vg, theme->getFrameColorBase());
				nvgStrokeWidth(vg, 1.f);
				nvgStroke(vg);
				if (useCaching) {
					notesView.data->SaveFill(vg, 3);
				}
			}
		}
		nvgCachePath(vg, 0);
	}

	if (useCaching && notesView.data) {
		notesView.data->pos = posContents;
		notesView.data->size = sizeContents;
		notesView.data->notesRendered = notesRendered;
		notesView.curRevision = notesView.reqRevision;
	}
	daw_tls::getTls().renderStats.clipsRendered++;
	daw_tls::getTls().renderStats.notesRendered+=notesRendered;
}
