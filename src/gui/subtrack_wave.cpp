#include "trackcontent.h"
#include "subtrack.h"
#include "basectrl.h"
#include "host/mainctrl.h"
#include "subtrack.h"
#include <nanovg.h>
#include "str_util.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "gui/drawwaveform.h"
#include "track.h"
#include "track_impl.h"
#include "color_util.h"
#include <unordered_map>

struct wave_split_layout_t {
	ivec2 pos{0};
	ivec2 size{0};
};
inline bool operator==(const wave_split_layout_t& lhs, const wave_split_layout_t& rhs){
	return lhs.pos == rhs.pos && lhs.size == rhs.size;
}
inline bool operator!=(const wave_split_layout_t& lhs, const wave_split_layout_t& rhs){
	return !operator==(lhs,rhs);
}
class gui_subtrack_waveview : public gui_track_subtrack {
	bool culled = true;
	scaled_grid& grid;
	struct waveform_layout_updated_t {
		audioclip_texture_t waveform;
		wave_split_layout_t layout;
	};
	struct waveview_entry {
		bool flagUpdated = false;
		int64_t sampleVersion = -1;
		gui_waveform_texture_ref waveformTex;
		audioclip_texture_t waveformUpdated;
		wave_split_layout_t layoutCurrent;
		wave_split_layout_t layoutUpdated;
		std::shared_ptr<audiotrack_split_t> sample;
	};
	std::unordered_map<int32_t, waveview_entry> splits;
	int32_t tickOffset = 0;
	int32_t updateCalls = 0;
public:
	gui_subtrack_waveview(track_gui_entry_t* _entry, DawCtrl* ctrl) : gui_track_subtrack(_entry, ctrl->getGrid(), nullptr, 0), grid(ctrl->getGrid()) {

	}
	~gui_subtrack_waveview() {
		for (auto& entry : splits) {
			auto& waveformTex = entry.second.waveformTex;
			if (waveformTex.rendered) {
				waveformrender* renderer = waveformrender::getInstance();
				if (renderer) {
					waveformrender::getInstance()->release(&waveformTex);
				}
			}
		}
	}
	virtual int subtrackType() override { return SUBTRACK_TYPE_WAVE; }

	void onTick(AppCtrl* appctrl) override {
		if (culled) {
			return;
		}
		if (tickOffset++>60) {
			tickOffset = 0;
			ivec2 ts = { 0, 0 };
			updatePosition(DawInstance::get()->getGlobals(), grid, ts, false);
		}
	}
	void render(NVGcontext* vg) override {
//		nvgBeginPath(vg);
//		nvgRect(vg, pos.x, pos.y, size.x, size.y);
//		nvgFillColor(vg, rgbToNvg(0xff00ff));
//		nvgFill(vg);

		if (!culled) {
			ivec2 posClipped = pos;
			ivec2 sizeClipped = size;
			this->parent->scissorClip(posClipped, sizeClipped);
			sizeClipped.y = size.y;
			nvgSave(vg);
			nvgTranslate(vg, pos.x, pos.y);
			int colorIdx = 0;
			static NVGcolor dbgcolorsa[5] = {
				nvgRGBA(255, 0, 0, 55),
				nvgRGBA(0, 255, 0, 55),
				nvgRGBA(0, 0, 255, 55),
				nvgRGBA(255, 0, 255, 55),
				nvgRGBA(255, 255, 0, 55)
			};

			for (auto& entry : splits) {
				auto& wv = entry.second.layoutCurrent;
				auto& waveformTex = entry.second.waveformTex;
				ivec2 wvSize = wv.size;
				if (waveformTex.waveform.size.x > 4 && waveformTex.waveform.size.y > 4 && wvSize.x > 4 && wvSize.y>4 && waveformTex.rendered) {
					nvgSave(vg);
						nvgTranslate(vg, wv.pos.x, wv.pos.y);

//						nvgBeginPath(vg);
//							nvgRect(vg, 0, 0, wvSize.x, wvSize.y);
//							nvgFillColor(vg, rgbToNvg(0xFFFFFF));
//						nvgFill(vg);
//						nvgBeginPath(vg);
//							nvgRect(vg, 2, 2, wvSize.x-4, wvSize.y-4);
//							nvgFillColor(vg, dbgcolorsa[colorIdx++ % 5]);
//						nvgFill(vg);
						ivec2 s = waveformTex.waveform.size;
						waveformrender::getInstance()->draw(vg, &waveformTex, s);

					nvgRestore(vg);
				}
			}
			nvgRestore(vg);

		}

		nvgSave(vg);
		automation.render(vg);
		nvgRestore(vg);
	}
	void refreshWaveform(waveview_entry* wv) {
		waveformrender::getInstance()->release(&wv->waveformTex);
		wv->flagUpdated = false;
		wv->waveformTex.rendered = false;
		if (wv->waveformUpdated.size.x > 0) {
			dbgassert(wv->layoutUpdated.size.x > 0);
		}
		wv->sampleVersion = wv->sample->version;
		wv->layoutCurrent = wv->layoutUpdated;
		wv->waveformTex.waveform = wv->waveformUpdated;
		dbgassert(wv->sampleVersion == wv->sample->version);
		dbgassert(!wv->waveformTex.queued);
		dbgassert(wv->waveformTex.waveform.size.x > 0 && wv->waveformTex.waveform.size.y > 0);
		updateCalls++;
		waveformrender::getInstance()->queueUpdate(wv->sample.get(), &wv->waveformTex);
	}
	void prerender(NVGcontext* vg) {
		for (guibase* gui : guis) {
			gui->prerender(vg);
		}

		for (auto& entry : splits) {
			auto& wv = entry.second;
			auto& waveformTex = wv.waveformTex;
			if (!waveformTex.queued) {
				if (!wv.sample || wv.waveformUpdated.size.x < 1 || wv.waveformUpdated.size.y < 1) {
					continue;
				}
				if (wv.flagUpdated) {
					refreshWaveform(&entry.second);
				}
			}
		}
		erase_if(splits, [](const auto& entry) {
			auto& waveformTex = entry.second.waveformTex;
			return !waveformTex.rendered && !waveformTex.queued;
		});
	}
	waveform_layout_updated_t makeWaveformFromClip(const project_globals_t& project, scaled_grid& grid,
			const waveview_entry& entry, ivec2& pos, ivec2& size, ivec2& posClipped, ivec2& sizeClipped) {


		samplerate_t sr = vsthost::getInstance()->sampleFormat.sampleRate; //TODO: store in project_t
		dbgassert(pos.x==0);


		double tickScreenStart = grid.screenToTickD(left());
		double tickScreenEnd = grid.screenToTickD(right());
		int64_t samplesScreenLen = math::ceilCast(tickToSamplePrecise(tickScreenEnd-tickScreenStart, project.tempo100, sr));

		double tickRenderStart = sampleToTickPrecise(entry.sample->samplePos, project.tempo100, sr);
		double tickRenderLen = sampleToTickPrecise(entry.sample->sample.nSamples, project.tempo100, sr);

		double samplesPerPx = samplesScreenLen/(double)size.x;

		double pxPerSample = 1.0/samplesPerPx;
		constexpr double MAX_RES = 2048;

		double posStart = math::max(0.0, grid.tickToScreenD(tickRenderStart));
		double posEnd = math::min((double)size.x, grid.tickToScreenD(tickRenderStart+tickRenderLen));
		double renderSize = posEnd-posStart;


		int64_t sampleBegin = 0;
		int64_t sampleBeginOffset = math::floorCast(math::max(0.0, tickToSamplePrecise(tickScreenStart-tickRenderStart, project.tempo100, sr)));

		int64_t nSamples = math::ceilCast(tickToSamplePrecise(grid.screenToTickD(posEnd), project.tempo100, sr) -
				tickToSamplePrecise(grid.screenToTickD(posStart), project.tempo100, sr));

		dbgassert(posEnd>posStart);


		waveform_layout_updated_t newentry;
		newentry.layout.pos = {math::floor(posStart), 0};
		newentry.layout.size = {math::floor(renderSize), size.y};



		audioclip_texture_t& w = newentry.waveform;
		w.quality=1;
		w.scaleX = 1.0f;
		w.pos = pos;

//		log_printf("sampleStartOffset %f, sampleBegin %f, sampleEnd %f\n", sampleStartOffset, sampleBegin, sampleEnd);
//		log_printf("lenViewSamples %f, nSamples %f, samples/px %f\n", lenViewSamples, nSamples, samplesPerPx);
		w.size = ivec2(0, math::min(size.y, FBO_HEIGHT));

		if (!FitsTypeRange<decltype(w.size.x)>(nSamples * pxPerSample) || nSamples * pxPerSample > FBO_WIDTH) {
			w.size.x = FBO_WIDTH;
			samplesPerPx = (nSamples / FBO_WIDTH);
//			my_printf("nSamples * pxPerSample > FBO_WIDTH  samplesPerPx = %f, size.x %d\n", samplesPerPx, w.size.x);
		} else {
			w.size.x = math::min((int32_t)math::floor(nSamples * pxPerSample), FBO_WIDTH);
//			my_printf("nSamples * pxPerSample < FBO_WIDTH  samplesPerPx = %f, size.x %d\n", samplesPerPx, w.size.x);
		}
		if (samplesPerPx > MAX_RES && (nSamples / MAX_RES) <= FBO_WIDTH) {
			w.scaleX = MAX_RES/samplesPerPx;
			samplesPerPx = MAX_RES;
//			my_printf("w.scaleX %f  samplesPerPx = %f\n", w.scaleX, samplesPerPx);
		}
		dbgassert(w.size.x <= FBO_WIDTH && w.size.y <= FBO_HEIGHT);
//		dbgassert(w.size.x > 0);
		if (w.size.x > 0) {
//			dbgassert(newentry.layout.size.x > 0);
		}
		w.sampleBegin = sampleBegin;
		w.sampleBeginOffset = sampleBeginOffset;
		w.sampleEnd = entry.sample->sample.nSamples;
		w.samplesPerPx = samplesPerPx;
		w.linewidth = 1.5f;//+min(0.75, max(0.0, grid.zoom*32.0));
		w.method = SampleMethod::sample_straight;
		w.audioId = entry.sample->sampleId;
		w.clipped = false;


		return newentry;

	}
	void updatePosition(const project_globals_t& globals, scaled_grid& grid, ivec2& trackSize, bool throttleRefresh) override {

//		size = this->parent->size;
		culled = size.x < 1 || size.y < 1;//!getClipPosition(grid, trackSize, m_clip, pos, size, 0);
		if (culled) {
			for (auto& entry : splits) {
				auto& waveformTex = entry.second.waveformTex;
				if (!waveformTex.queued) {
					waveformrender::getInstance()->release(&waveformTex);
					waveformTex.rendered = false;
				}
			}
		}

		if (!culled) {
			dbgassert(size.x > 0);
			ivec2 clipSize = ivec2(size.x, size.y);
			ivec2 posClipped = pos;
			ivec2 sizeClipped = clipSize;
			this->parent->scissorClip(posClipped, sizeClipped);
			sizeClipped.y = clipSize.y;
			double tickBegin = grid.screenToTickD(pos.x);
			double tickEnd = grid.screenToTickD(pos.x + size.x);
			samplerate_t sr = vsthost::getInstance()->sampleFormat.sampleRate; //TODO: store in project_t
			double trackPosSampleStart = tickToSamplePrecise(tickBegin, globals.tempo100, sr);
			double trackPosSampleEnd = tickToSamplePrecise(tickEnd, globals.tempo100, sr);
			if (posClipped.x+sizeClipped.x <= 0 || sizeClipped.x <= 0) {
				culled = true;
			} else {
				std::vector<audiotrack_split_t*> samples;
				std::vector<int32_t> samplesPresent;
				this->m_track->audio->audioOutput.visitSamples([&samples, &samplesPresent, &trackPosSampleStart, &trackPosSampleEnd](const std::shared_ptr<audiotrack_split_t>& split) {
					if (split && split->samplePos < trackPosSampleEnd && split->samplePos+split->getSample()->nSamples > trackPosSampleStart) {
						samples.push_back(split.get());
						samplesPresent.push_back(split->sampleId);
					}
				});
				for (auto& sample : samples) {
					if (!this->splits.count(sample->sampleId)) {
						splits[sample->sampleId] = waveview_entry();
					}
					waveview_entry& entry = this->splits[sample->sampleId];
					auto& texture = entry.waveformTex;
					if (!texture.queued) {
						entry.sample = this->m_track->audio->audioOutput.getSampleById(sample->sampleId);
						waveform_layout_updated_t updatedEntry = makeWaveformFromClip(globals, grid, entry, pos, clipSize, posClipped, sizeClipped);
						dbgassert(updatedEntry.waveform.audioId >= 0 || entry.sample.get() == nullptr);
						if (updatedEntry.waveform.audioId < 0
								|| updatedEntry.waveform.size.x < 1
								|| updatedEntry.waveform.size.y < 1
								|| updatedEntry.layout.size.x < 1
								|| updatedEntry.layout.size.y < 1) {
							waveformrender::getInstance()->release(&entry.waveformTex);
							entry.flagUpdated = false;
							entry.waveformTex.rendered = false;
							entry.waveformTex.waveform = updatedEntry.waveform;
							entry.layoutCurrent = updatedEntry.layout;
							entry.waveformUpdated = updatedEntry.waveform;
							entry.layoutUpdated = updatedEntry.layout;
						} else {
							bool equal = sample->version == entry.sampleVersion
									&& ((updatedEntry.waveform.size.y > 0) == (texture.waveform.size.y > 0))
									&& updatedEntry.waveform == texture.waveform
									&& updatedEntry.layout == entry.layoutCurrent;
							if (sample->version != entry.sampleVersion) {
//								my_printf("sample->version != entry.sampleVersion %d %d\n", sample->version, equal);
							}
							bool canQueue = waveformrender::getInstance()->canQueueUpdate();
							ivec2 sizeDiff = math::absvec2(updatedEntry.waveform.size-texture.waveform.size);
							ivec2 limit = math::maxvec2(ivec2(1), ivec2(updatedEntry.waveform.size.x/4, 16));
							if (!canQueue) {
								limit.x = updatedEntry.waveform.size.x/4;
							}
							if (updatedEntry.waveform.clipped || !throttleRefresh) {
								limit = {0,0};
							}
							if (!equal || (sizeDiff.x > limit.x || sizeDiff.y > limit.y)) {
		//						if (!equal)
		//							my_printf("unequal\n",0);
		//						else {
		//							my_printf("sizeDiff %d,%d / %d,%d (canQueue %d)\n",sizeDiff.x,sizeDiff.y,limit.x,limit.y, canQueue);
		//						}
								entry.waveformUpdated = updatedEntry.waveform;
								entry.layoutUpdated = updatedEntry.layout;
								entry.flagUpdated = true;
								if (sizeDiff.x > limit.x || sizeDiff.y > limit.y) {
									waveformrender::getInstance()->release(&entry.waveformTex);
									entry.waveformTex.rendered = false;
								}
							}
						}
					}
				}
				erase_if(splits, [&samplesPresent](const auto& entry) {
					return !stl_contains(samplesPresent, entry.second.sample->sampleId);
				});
			}

		}

	}
	void renderMixerInfo(NVGcontext* vg) override {
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		gui_track_subtrack::renderMixerInfo(vg);
		const int htt = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		const int titleHeight = htt*4/5;
		const int fontSize = titleHeight-4;
		int32_t y = INSET_TITLE;
		y += titleHeight*2;
		setFont(vg, fontSize, G_WHITE, G_TITLE_ALIGN);
		renderText(vg, 0 + INSET_TITLE, y+titleHeight / 2, size.x, StringAsCStr(StringFormat("updateCalls: %d", updateCalls)));
		y+=titleHeight;
		renderText(vg, 0 + INSET_TITLE, y+titleHeight / 2, size.x, StringAsCStr(StringFormat("culled: %s", culled?"true":"false")));
		y+=titleHeight;
		renderText(vg, 0 + INSET_TITLE, y+titleHeight / 2, size.x, StringAsCStr(StringFormat("Splits: %d", splits.size())));
		y+=titleHeight;
		//TODO: Next line is not thread-safe
		renderText(vg, 0 + INSET_TITLE, y+titleHeight / 2, size.x, StringAsCStr(StringFormat("samples.size: %d", this->m_track->audio->audioOutput.samples.size())));
		y+=titleHeight;
		//TODO: Next line is not thread-safe
		renderText(vg, 0 + INSET_TITLE, y+titleHeight / 2, size.x, StringAsCStr(StringFormat("data.size: %d", this->m_track->audio->audioOutput.data.size())));


	}
};

gui_track_subtrack* makeGuiSubtrack(track_gui_entry_t* entry, DawCtrl* ctrl, int type) {

	return new gui_subtrack_waveview(entry, ctrl);
}
