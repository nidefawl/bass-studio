#include "gui/track/trackcontent.h"
#include "subtrack.h"
#include "basectrl.h"
#include "host/daw/mainctrl.h"
#include <nanovg.h>
#include "str_util.h"
#include "math/vec.h"
#include "math/seq_math.h"
#include "wave/waveform_render_impl.h"
#include "host/track/track.h"
#include "host/track/track_impl.h"
#include "color_util.h"
#include "seq_util.h"
#include <unordered_map>

class gui_subtrack_waveview final : public gui_track_subtrack {
    struct waveview_entry {
        bool flagUpdated      = false;
        int64_t sampleVersion = -1;
        gui_waveform_texture_ref waveformTex;
        audioclip_texture_t waveformUpdated;
        wave_split_layout_t layoutCurrent;
        wave_split_layout_t layoutUpdated;
        std::shared_ptr<audiotrack_split_t> sample;
    };
    bool culled = true;
    std::unordered_map<int64_t, waveview_entry> splits;
    int32_t tickOffset  = 0;
    int32_t updateCalls = 0;

    std::vector<audiotrack_split_t*> waveviewSamplesPresent;
    std::vector<int64_t> waveviewSampleIdsPresent;
public:
    gui_subtrack_waveview(track_gui_entry_t* _entry, scaled_grid& grid)
        : gui_track_subtrack(_entry, grid, nullptr, 0) {
    }
    ~gui_subtrack_waveview() override {
        for (auto& entry : splits) {
            auto& waveformTex = entry.second.waveformTex;
            if (waveformTex.rendered) {
                waveformrender* renderer = dawCtrl->getWaveformRenderer();
                if (renderer) {
                    renderer->release(&waveformTex);
                }
            }
        }
    }
    int subtrackType() override { return SUBTRACK_TYPE_WAVE; }

    void onTick(AppCtrl*) override {
        if (culled) {
            return;
        }
        if (tickOffset++ > 60) {
            tickOffset = 0;
            ivec2 ts   = { 0, 0 };
            updatePosition(dawCtrl->getDaw()->getGlobals(), getGrid(), ts, false);
        }
    }

    void renderDebugPass(NVGcontext* vg) override {
        int colorIdx                  = 0;

        nvgSave(vg);
        nvgTranslate(vg, pos.x, pos.y);
        for (auto& entry : splits) {
            auto& wv = entry.second;
            auto& waveformTex = wv.waveformTex;
            auto& wvLC = wv.layoutCurrent;
            //nvgTranslate(vg, wv.splitTexPos.x, wv.splitTexPos.y);

            nvgBeginPath(vg);
            nvgRect(vg, wvLC.splitTexPos.x, wvLC.splitTexPos.y, wvLC.spliTexSize.x, wvLC.spliTexSize.y);
            NVGcolor bgWave = dbgcolorsArray[colorIdx % 5];
            bgWave.a        = 0.3f;

            nvgFillColor(vg, bgWave);
            nvgFill(vg);

            if (wv.waveformTex.rendered) {
                nvgSave(vg);
                nvgTranslate(vg, wvLC.splitTexPos.x, wvLC.splitTexPos.y);
                dawCtrl->getWaveformRenderer()->draw(vg, &waveformTex, wvLC.spliTexSize);
                nvgRestore(vg);
            }

            colorIdx++;
        }

        nvgRestore(vg);
    }

    void render(NVGcontext* vg) override {
        if (!isRenderableSizeAndContext(vg))
            return;

        if (!culled) {

            nvgSave(vg);
            nvgTranslate(vg, pos.x, pos.y);

            for (auto& entry : splits) {
                auto& wv          = entry.second.layoutCurrent;
                auto& waveformTex = entry.second.waveformTex;
                ivec2 wvSize      = wv.spliTexSize;
                if (waveformTex.waveform.size.x > 4 && waveformTex.waveform.size.y > 4 && wvSize.x > 4 && wvSize.y > 4 && waveformTex.rendered) {
                    nvgSave(vg);
                    nvgTranslate(vg, wv.splitTexPos.x, wv.splitTexPos.y);
                    dawCtrl->getWaveformRenderer()->draw(vg, &waveformTex, wv.spliTexSize);

                    nvgRestore(vg);
                }
            }
            nvgRestore(vg);
        }

        nvgSave(vg);
        guiTrAutomation.render(vg);
        nvgRestore(vg);
    }
    void refreshWaveform(waveview_entry* wv) {
        wv->layoutCurrent = wv->layoutUpdated;
        updateCalls++;
        if (!wv->waveformTex.queued) {
            wv->waveformTex.waveform = wv->waveformUpdated;
            wv->sampleVersion        = wv->sample->version;
            wv->flagUpdated          = false;

            //log_lf(Log::L_DEBUG, "split[%zd] version %zd update!\n", wv->sample->sampleId, wv->sampleVersion);
            if (!dawCtrl->getWaveformRenderer()->queueUpdate(wv->sample.get(), &wv->waveformTex)) {
                log_lf(Log::L_WARN, "Failed queuing waveform update\n");
            }
        }
    }

    void prerender(NVGcontext* vg) override {
        for (guibase* gui : guis) {
            gui->prerender(vg);
        }

        for (auto& entry : splits) {
            auto& wv = entry.second;
            if (!wv.sample || wv.waveformUpdated.size.x < 1 || wv.waveformUpdated.size.y < 1) {
                continue;
            }
            if (wv.flagUpdated) {
                refreshWaveform(&entry.second);
            }
        }
        erase_if(splits, [](const auto& entry) {
            auto& waveformTex = entry.second.waveformTex;
            return !waveformTex.rendered && !waveformTex.queued;
        });
    }
    bool makeWaveformFromWaveview(const int32_t tempo100, const samplerate_t samplerate,
                                  const waveview_entry& entry,
                                  const ivec2& pos, const ivec2& size,
                                  waveform_layout_updated_t& out) {
        dbgassert(pos.x == 0);

        const double tickRenderStart = sampleToTickConvert<double, roundmode::none>(entry.sample->samplePos, tempo100, samplerate);
        const double tickRenderLen = sampleToTickConvert<double, roundmode::none>(entry.sample->sample.nSamples, tempo100, samplerate);

        auto& grid = getGrid();
        const double posStart    = grid.tickToScreenD(tickRenderStart);
        double renderSizeX = grid.tickLenToScreen(tickRenderLen);

        ivec2 posClipped = { math::floordS32(posStart), 0 };
        ivec2 sizeClipped = { math::ceildS32(renderSizeX), size.y };

        bool wasClipped = getClippedPosSize(parent->size, posClipped, sizeClipped);

        if (posClipped.x + sizeClipped.x <= 0 || sizeClipped.x <= 0) {
            return false;
        }

        double nSamples = static_cast<double>(entry.sample->sample.nSamples);

        const int64_t sampleBegin       = 0;
        int64_t sampleBeginOffset = 0;

        if (wasClipped) {
            double clippedStartTick = grid.screenToTickD(posClipped.x);
            double clippedEndTick = grid.screenToTickD(posClipped.x + sizeClipped.x);

            auto sampleScreenPosStart = tickToSampleConvert<double, roundmode::none>(clippedStartTick, tempo100, samplerate);
            auto sampleScreenPosEnd   = tickToSampleConvert<double, roundmode::none>(clippedEndTick, tempo100, samplerate);


            sampleBeginOffset = tickToSampleConvert<int64_t, roundmode::floorclamp>(clippedStartTick - tickRenderStart, tempo100, samplerate);


            nSamples = math::ceild(sampleScreenPosEnd - sampleScreenPosStart);
            renderSizeX = static_cast<double>(sizeClipped.x);
        }

        waveform_layout_updated_t newentry;
        newentry.layout.splitTexPos = posClipped;
        newentry.layout.spliTexSize = sizeClipped;

        audioclip_texture_t& w = newentry.waveform;

        w.quality = 1;
        w.scaleX  = 1.0f;
        w.pos     = pos;
        w.size    = ivec2(0, math::min(size.y, FBO_HEIGHT));

        constexpr double MAX_RES     = 2048;
        constexpr double FBO_WIDTH_D = FBO_WIDTH;

        double samplesPerPx   = nSamples / renderSizeX;
        double pxPerSample    = 1.0 / samplesPerPx;

        if (!FitsTypeRange<decltype(w.size.x)>(nSamples * pxPerSample) || nSamples * pxPerSample > FBO_WIDTH_D) {
            w.size.x     = FBO_WIDTH;
            samplesPerPx = (nSamples / FBO_WIDTH_D);
        } else {
            w.size.x = math::min(math::floordS32(nSamples * pxPerSample), FBO_WIDTH);
        }

        if (samplesPerPx > MAX_RES && (nSamples / MAX_RES) <= FBO_WIDTH_D) {
            w.scaleX     = static_cast<float>(MAX_RES / samplesPerPx);
            samplesPerPx = MAX_RES;
        }

        dbgassert(w.size.x <= FBO_WIDTH && w.size.y <= FBO_WIDTH_D);

        w.sampleBegin       = sampleBegin;
        w.sampleBeginOffset = sampleBeginOffset;
        w.sampleEnd         = entry.sample->sample.nSamples;
        w.samplesPerPx      = samplesPerPx;
        w.linewidth         = 2;//+min(0.75, max(0.0, grid.zoom*32.0));
        w.method            = SampleMethod::sample_straight;
        w.audioId           = entry.sample->sampleId;
        w.sampleVersion     = entry.sample->version;
        w.clipped           = false;

        out = newentry;
        return true;
    }

    void updatePosition(const project_globals_t& globals, scaled_grid& grid, ivec2& trackSize, bool throttleRefresh) override {


        culled = size.x < 1 || size.y < 1;//!getClipPosition(grid, trackSize, m_clip, pos, size, 0);

        if (culled) {
            for (auto& entry : splits) {
                auto& waveformTex = entry.second.waveformTex;
                if (!waveformTex.queued) {
                    dawCtrl->getWaveformRenderer()->release(&waveformTex);
                    waveformTex.rendered = false;
                }
            }
            return;
        }

        double tickBegin = grid.screenToTickD(pos.x);
        double tickEnd   = grid.screenToTickD(pos.x + size.x);
        const auto sr = this->m_track->audio->sampleFormat.sampleRate;
        const int32_t tempo100 = globals.tempo100;

        int64_t trackPosSampleStart = tickToSampleConvert<int64_t, roundmode::floor>(tickBegin, tempo100, sr);
        int64_t trackPosSampleEnd   = tickToSampleConvert<int64_t, roundmode::ceil>(tickEnd, tempo100, sr);

        waveviewSamplesPresent.clear();
        waveviewSampleIdsPresent.clear();

        this->m_track->audio->audioOutput.visitSamples(
            [this, &trackPosSampleStart, &trackPosSampleEnd]
            (const std::shared_ptr<audiotrack_split_t>& split) {
                if (split && split->samplePos < trackPosSampleEnd && split->samplePos + split->getSample()->nSamples > trackPosSampleStart) {
                    waveviewSamplesPresent.push_back(split.get());
                    waveviewSampleIdsPresent.push_back(split->sampleId);
                }
            }
        );

        for (auto& sample : waveviewSamplesPresent) {
            if (!this->splits.count(sample->sampleId)) {
                splits[sample->sampleId] = waveview_entry();
            }
            waveview_entry& entry = this->splits[sample->sampleId];
            auto& texture         = entry.waveformTex;

            entry.sample = this->m_track->audio->audioOutput.getSampleById(sample->sampleId);
            waveform_layout_updated_t updatedEntry;
            bool samplesVisible = makeWaveformFromWaveview(tempo100, sr, entry, pos, size, updatedEntry);
            if (!samplesVisible
                || updatedEntry.waveform.audioId < 0
                || updatedEntry.waveform.size.x < 1
                || updatedEntry.waveform.size.y < 1
                || updatedEntry.layout.spliTexSize.x < 1
                || updatedEntry.layout.spliTexSize.y < 1)
            {
                dawCtrl->getWaveformRenderer()->release(&entry.waveformTex);
                entry.flagUpdated          = false;
                entry.waveformTex.rendered = false;
                entry.waveformTex.waveform = updatedEntry.waveform;
                entry.layoutCurrent        = updatedEntry.layout;
                entry.waveformUpdated      = updatedEntry.waveform;
                entry.layoutUpdated        = updatedEntry.layout;
                continue;
            }

            dbgassert(updatedEntry.waveform.audioId >= 0 || entry.sample.get() == nullptr);

            bool equal = math::abs((sample->version - entry.sampleVersion)) < 1
                         && ((updatedEntry.waveform.size.y > 0) == (texture.waveform.size.y > 0))
                         && isEqualWaveform3(updatedEntry.waveform, texture.waveform)
                         && updatedEntry.layout == entry.layoutCurrent;

            bool canQueue  = dawCtrl->getWaveformRenderer()->canQueueUpdate();
            ivec2 sizeDiff = math::absvec2(updatedEntry.waveform.size - texture.waveform.size);
            ivec2 limit    = math::maxvec2(ivec2(1), ivec2(updatedEntry.waveform.size.x / 4, 16));

            if (!canQueue) {
                limit.x = updatedEntry.waveform.size.x / 4;
            }

            if (updatedEntry.waveform.clipped || !throttleRefresh) {
                limit = { 0, 0 };
            }

            if (!equal || (sizeDiff.x > limit.x || sizeDiff.y > limit.y)) {
                entry.waveformUpdated = updatedEntry.waveform;
                entry.layoutUpdated   = updatedEntry.layout;
                entry.flagUpdated     = true;
                if (sizeDiff.x > limit.x || sizeDiff.y > limit.y) {
                    //dawCtrl->getWaveformRenderer()->release(&entry.waveformTex);
                    //entry.waveformTex.rendered = false;
                    //log_lf(Log::L_DEBUG, "layoutUpdated pos %d %d\n", entry.layoutUpdated.splitTexPos.x, entry.layoutUpdated.splitTexPos.y);
                }
            }
        }

        for (auto it = splits.begin(); it != splits.end();) {
            auto& entry = *it;
            if (!stl_contains(waveviewSampleIdsPresent, entry.second.sample->sampleId)) {
                waveview_entry& waveviewEntry         = entry.second;
                gui_waveform_texture_ref* waveformRef = &waveviewEntry.waveformTex;
                dawCtrl->getWaveformRenderer()->release(waveformRef);
                it = splits.erase(it);
            } else {
                ++it;
            }
        }

    }

    void renderMixerInfo(NVGcontext* vg, ivec2 pos, ivec2 size) override {
        ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
        gui_track_subtrack::renderMixerInfo(vg, pos, size);
        const int htt         = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
        const int fontSize    = htt;
        for (int i = 0; i < 5; ++i) {
            String strInfo;
            switch (i) {
                case 0:
                    strInfo = StringFormat("updateCalls: %d", updateCalls);
                    break;
                case 1:
                    strInfo = StringFormat("culled: %s", culled ? "true" : "false");
                    break;
                case 2:
                    strInfo = StringFormat("Splits: %zu", splits.size());
                    break;
                case 3:
                    //TODO: Next line is not thread-safe
                    strInfo = StringFormat("samples.size: %zu", this->m_track->audio->audioOutput.samples.size());
                    break;
                case 4:
                    //TODO: Next line is not thread-safe
                    strInfo = StringFormat("data.size: %zu", this->m_track->audio->audioOutput.data.size());
                    break;
            }
            renderTextLabel(vg, 
                vec2(0, htt * (0.5f + i + 2)) + vec2(INSET_TITLE),
                vec2(size.x - INSET_TITLE, htt),
                strInfo,
                theme, fontSize, theme->getColor(GuiColor::COL_WHITE), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        }
    }
};

gui_track_subtrack* makeGuiSubtrack(track_gui_entry_t* entry, scaled_grid& grid, int type) {
    return new gui_subtrack_waveview(entry, grid);
}
