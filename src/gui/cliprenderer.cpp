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
	dbgassert(FitsTypeRange<int32_t>(tickBeginX));
	dbgassert(FitsTypeRange<int32_t>(tickEndX));
	int32_t tickBeginPx = (int32_t) round(tickBeginX);
	int32_t widthPx = (int32_t) round(width);
	pos = ivec2(tickBeginPx, INSET_TRACK_CONTENT);
	size = ivec2(widthPx, size.y-INSET_TRACK_CONTENT*2);
	return size.x > 0 && size.y > 0;
}
audioclip_texture_t makeWaveformFromClip(project_t& project, scaled_grid& grid,
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
		nvgStrokeColor(vg, tr->content->theme->getFrameColorBase());
		nvgStrokeWidth(vg, 1.f);
		nvgStroke(vg);
	}
}
float noteToScreen(float note, float scale, float offset, float sizeY) {
	float offsetKey = note * scale;
	float rel = offsetKey - offset;
	return (sizeY) - rel;
}

struct noteview_cache_impl_t {
	bool valid = false;
	int64_t notesRendered;
	ivec2 pos;
	ivec2 size;
	nvg_path_cache_storage_t* cache1 = nullptr;
	nvg_path_cache_storage_t* cache2 = nullptr;
	nvg_path_cache_storage_t* cache3 = nullptr;
	nvg_path_cache_storage_t* cache4 = nullptr;
	~noteview_cache_impl_t() {
		reset();
	}
	void reset() {
		valid = false;
		if (cache1) {
			nvgReleaseCacheResult(cache1); cache1 = nullptr;
		}
		if (cache2) {
			nvgReleaseCacheResult(cache2); cache2 = nullptr;
		}
		if (cache3) {
			nvgReleaseCacheResult(cache3); cache3 = nullptr;
		}
		if (cache4) {
			nvgReleaseCacheResult(cache4); cache4 = nullptr;
		}
	}
};
noteview_render_t::~noteview_render_t() {
	if (data) {
		delete data;
	}
}
void renderMidiClip(NVGcontext* vg, const guitheme_t* theme, const track_t* tr, const clip_t* cl, ivec2 pos, ivec2 size) {
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
	cacheValid &= notesView.data != nullptr && notesView.data->cache1 != nullptr;
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
		nvgTranslate(vg, posContents.x, posContents.y);
		if (notesView.data->cache1) {
			nvgFillFromCache(vg, notesView.data->cache1);
		}
		if (notesView.data->cache2) {
			nvgFillFromCache(vg, notesView.data->cache2);
		}
		if (notesView.data->cache3) {
			nvgFillFromCache(vg, notesView.data->cache3);
		}
		notesRendered += notesView.data->notesRendered;
		nvgRestore(vg);
		if (notesView.data->cache4) {
			nvgFillFromCache(vg, notesView.data->cache4);
		}
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
						nvgGetLastCacheResult(vg, &notesView.data->cache1);
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
								nvgGetLastCacheResult(vg, &notesView.data->cache2);
							} else {
								nvgGetLastCacheResult(vg, &notesView.data->cache3);
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
				nvgStrokeColor(vg, tr->content->theme->getFrameColorBase());
				nvgStrokeWidth(vg, 1.f);
				nvgStroke(vg);
				if (useCaching) {
					nvgGetLastCacheResult(vg, &notesView.data->cache4);
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
