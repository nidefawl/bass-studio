#include "trackcontent.h"
#include "subtrack.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "subtrack.h"
#include <nanovg.h>
#include "math/vec.h"
#include "math/seq_math.h"
#include "gui/drawwaveform.h"
#include "track.h"
#include "track_impl.h"

class gui_subtrack_waveview : public gui_track_subtrack {
	gui_waveform_texture_ref waveformRef;
	audioclip_texture_t updatedWaveform;
	bool culled = true;
	scaled_grid& grid;
	std::shared_ptr<audiotrack_split_t> current;
public:
	gui_subtrack_waveview(track_t* mtrack, MainCtrl* ctrl) : gui_track_subtrack(mtrack, ctrl->getGrid(), nullptr, 0), grid(ctrl->getGrid()) {

	}
	~gui_subtrack_waveview() {
		if (waveformRef.rendered) {
			waveformrender* renderer = waveformrender::getInstance();
			if (renderer) {
				waveformrender::getInstance()->release(&waveformRef);
			}
		}
	}
	virtual int subtrackType() { return SUBTRACK_TYPE_WAVE; }


	void render(NVGcontext* vg) {
		if (MainCtrl::get()->getSelectedTrack() == m_track) {
		}
//		nvgBeginPath(vg);
//		nvgRect(vg, pos.x, pos.y, size.x, size.y);
//		nvgFillColor(vg, rgbToNvg(0xff00ff));
//		nvgFill(vg);

		if (!culled) {
			ivec2 posClipped = pos;
			ivec2 sizeClipped = size;
			this->parent->scissorClip(posClipped, sizeClipped);
			sizeClipped.y = size.y;

			if (waveformRef.waveform.size.x > 4 && waveformRef.waveform.size.y > 4 && waveformRef.rendered) {
				nvgSave(vg);
					nvgTranslate(vg, pos.x, pos.y);

					nvgBeginPath(vg);
						nvgRect(vg, 0, 0, waveformRef.waveform.size.x, waveformRef.waveform.size.y);
						nvgFillColor(vg, rgbToNvg(0xFFFFFF));
					nvgFill(vg);
					nvgBeginPath(vg);
						nvgRect(vg, 2, 2, waveformRef.waveform.size.x-4, waveformRef.waveform.size.y-4);
						nvgFillColor(vg, rgbToNvg(0x220022));
					nvgFill(vg);
					waveformrender::getInstance()->draw(vg, &waveformRef, waveformRef.waveform.size);

				nvgRestore(vg);
			}
		}

		nvgSave(vg);
		automation.render(vg);
		nvgRestore(vg);
	}
	void releaseRendered() {
	//	my_printf("release %012x from releaseRendered()\n", &waveformRef);
		waveformrender::getInstance()->release(&waveformRef);
	//	waveformRef.fbId = -1;
		waveformRef.rendered = false;
	}

	void prerender(NVGcontext* vg) {
		for (guibase* gui : guis) {
			gui->prerender(vg);
		}

		if (!waveformRef.queued) {
			if (!current || this->updatedWaveform.size.x < 1 || this->updatedWaveform.size.y < 1) {
				return;
			}
			if ((!waveformRef.rendered || (this->updatedWaveform != waveformRef.waveform))) {
				releaseRendered();
				waveformRef.waveform = this->updatedWaveform;
				assert(!waveformRef.queued);
				assert(waveformRef.waveform.size.x > 0 && waveformRef.waveform.size.y > 0);
				waveformrender::getInstance()->queueUpdate(current.get(), &waveformRef);
			}
		}
	}

	audioclip_texture_t makeWaveformFromClip(project_t& project, scaled_grid& grid,
			ivec2& trackSize, ivec2& pos, ivec2& size, ivec2& posClipped, ivec2& sizeClipped) {


		samplerate_t sr = vsthost::getInstance()->lSampleRate; //TODO: store in project_t
		assert(pos.x==0);
		int32_t pxEnd = posClipped.x + sizeClipped.x;
		double tickBegin = grid.screenToTickD(pos.x);
		double tickEnd = grid.screenToTickD(pos.x + size.x);
//		double tickBeginClipped = grid.screenToTickD(pxBegin);
//		double tickEndClipped = grid.screenToTickD(pxEnd);
//		if (size.x == sizeClipped.x) {
//			tickBeginOffset = m_clip->start();
//			tickBegin = m_clip->start();
//			tickEnd = m_clip->end();
//		}
		double trackPosSampleStart = tickToSamplePrecise(tickBegin, project.tempo100, sr);
		double trackPosSampleEnd = tickToSamplePrecise(tickEnd, project.tempo100, sr);
		double lenViewSamples = trackPosSampleEnd - trackPosSampleStart;
//		double sampleStartOffset = tickToSamplePrecise(tickBeginOffset, project.tempo100, sr);


		current = this->m_track->audio->audioOutput.getSample(trackPosSampleStart);

		audioclip_texture_t w;
		if (!current) {
			return w;
		}
		assert(current->samplePos <= trackPosSampleStart);
		int32_t pxBegin = grid.tickLenToScreen(sampleToTickPrecise(current->samplePos, project.tempo100, sr));

//		ivec2 startOffset = {math::max(0, pos.x - pxBegin), 0};
		ivec2 startOffset = {math::max(0, 0), 0};
		double sampleStartOffset = math::max(0.0, trackPosSampleStart-(double)current->samplePos);
		double sampleBegin = 0.0;//math::max(0.0, trackPosSampleStart-(double)current->samplePos);
		double sampleEnd = math::min(lenViewSamples+sampleStartOffset, (double)current->sample.nSamples);

		double samplesPerPx = lenViewSamples/size.x;




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
		w.startOffset = startOffset;
		double nSamples = lenViewSamples;
		log_printf("sampleStartOffset %f, sampleBegin %f, sampleEnd %f\n", sampleStartOffset, sampleBegin, sampleEnd);
		log_printf("lenViewSamples %f, nSamples %f, samples/px %f\n", lenViewSamples, nSamples, samplesPerPx);
		w.size = ivec2(math::min((int32_t)math::round(nSamples * pxPerSample), FBO_WIDTH), math::min(size.y, FBO_HEIGHT));
		if (nSamples * pxPerSample > FBO_WIDTH) {
			w.size.x = FBO_WIDTH;
			samplesPerPx = (nSamples / FBO_WIDTH);
			my_printf("nSamples * pxPerSample > FBO_WIDTH  samplesPerPx = %f\n", samplesPerPx);
		}
	//	if (w.scale == 1) {
	//		w.size.x = math::min(FBO_WIDTH, (int)std::round(fsizeX));
			if (samplesPerPx > MAX_RES && (nSamples / MAX_RES) <= FBO_WIDTH) {
				w.scaleX = MAX_RES/samplesPerPx;
				samplesPerPx = MAX_RES;
				my_printf("w.scaleX %f  samplesPerPx = %f\n", w.scaleX, samplesPerPx);
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
	//	assert(width <= FBO_WIDTH);
		assert(w.size.x <= FBO_WIDTH && w.size.y <= FBO_HEIGHT);
		assert(w.size.x > 0);
		w.sampleBegin = sampleBegin;
		w.sampleBeginOffset = sampleStartOffset;
		w.sampleEnd = sampleEnd;
		w.samplesPerPx = samplesPerPx;
		w.linewidth = 1.5f;//+min(0.75, max(0.0, grid.zoom*32.0));
		w.method = SampleMethod::sample_straight;
		w.audioId = current->sampleId;
		w.clipped = size.x != sizeClipped.x;
	//	my_printf("waveform[height:%d,zoom:%f,q:%d,w:%f,smp/px:%f,scale:%d]\n", w.size.y, grid.zoom, w.quality, w.linewidth, w.samplesPerPx, w.scale);


		return w;

	}
	void updatePosition(project_t& project, scaled_grid& grid, ivec2& trackSize) override {

//		size = this->parent->size;
		culled = false;//!getClipPosition(grid, trackSize, m_clip, pos, size, 0);
//		audiofile_t* audio = this->m_track->audio->audioOutput.getSample()
		if (culled) {
	//		my_printf("release %012x from updatePosition() (culled)\n", &waveformRef);
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
			ivec2 clipSize = ivec2(size.x, size.y-(HEIGHT_CLIP_TITLE+INSET_CLIP_CONTENT*2));
			ivec2 posClipped = pos;
			ivec2 sizeClipped = clipSize;
			this->parent->scissorClip(posClipped, sizeClipped);
			sizeClipped.y = clipSize.y;
			if (posClipped.x+sizeClipped.x <= 0 || sizeClipped.x <= 0) {
				culled = true;
			} else {
				auto waveform = makeWaveformFromClip(project, grid, trackSize, pos, clipSize, posClipped, sizeClipped);
				assert(waveform.audioId >= 0 || current.get() == nullptr);
				if (waveform.audioId < 0 || waveform.size.x < 1 || waveform.size.y < 1) {
					releaseRendered();
					waveformRef.waveform = waveform;
					this->updatedWaveform = waveform;
				} else {
					bool equal = ((waveform.size.y > 0) == (waveformRef.waveform.size.y > 0)) && isEqualWaveform3(waveform, waveformRef.waveform);

					bool canQueue = waveformrender::getInstance()->canQueueUpdate();
					ivec2 sizeDiff = math::absvec2(waveform.size-waveformRef.waveform.size);
					ivec2 limit = math::maxvec2(ivec2(1), ivec2(waveform.size.x/4, 16));
					if (!canQueue) {
						limit.x = waveform.size.x/4;
					}
					if (waveform.clipped || (MainCtrl::get() && !MainCtrl::get()->isZooming())) {
						limit = {0,0};
					}
					if (!equal || (sizeDiff.x > limit.x || sizeDiff.y > limit.y)) {
//						if (!equal)
//							my_printf("unequal\n",0);
//						else {
//							my_printf("sizeDiff %d,%d / %d,%d (canQueue %d)\n",sizeDiff.x,sizeDiff.y,limit.x,limit.y, canQueue);
//						}
						this->updatedWaveform = waveform;
						if (sizeDiff.x > limit.x || sizeDiff.y > limit.y) {
							releaseRendered();
						}
					}
				}
			}

		}

	}
};

gui_track_subtrack* makeGuiSubtrack(MainCtrl* ctrl, track_t* track, int type) {

	return new gui_subtrack_waveview(track, ctrl);
}
