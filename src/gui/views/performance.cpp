#include <algorithm>
#include <vector>
#include "gui/container/container_builder.hpp"
#include "math/seq_math.hpp"
#include "seq_util.hpp"
#include "str_util.hpp"
#include "util/profiling.hpp"
#include "util/profiling_impl.hpp"
#include "guicolors.hpp"
#include "guiconstant.hpp"
#include "renderresources.hpp"
#include "gui/gui.hpp"
#include "gui/container/container.hpp"
#include "gui/container/scrollcontainer.hpp"
#include "gui/controls/textfield.hpp"
#include "host/daw/mainctrl.hpp"
#include "host/host_pluginmanager.hpp"
#include "host/host.hpp"
#include "host/audiohost/audio_host.hpp"
#include "gui/meter/guimeter.hpp"
#include "appconfig.hpp"

class gui_performance_stats final : public guictr_base {
    int32_t minHTop = 66;
    host_stats_t stats{};
    int64_t timeLastUpdate = 0L;
    playback_state state{ status_stop };
    textlabel_dynamic_t label;

public:
    gui_performance_stats() : guictr_base() {
        setBackgroundRendered(true);
    }
    ~gui_performance_stats() override = default;
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        auto const daw = dawCtrl->getDaw();
        auto const host = daw->getHost();
        auto const audioHost = daw->getAudioHost();
        auto stream = audioHost ? audioHost->getStream(0) : nullptr;
        if (getTimeMicros() - timeLastUpdate >= 250000) {
            timeLastUpdate  = getTimeMicros();
            ThreadLock lock = daw->getPlayThread()->tryLockThread();
            if (lock.isLocked()) {
                state = daw->getPlayThread()->getState();
                host->getStats(stats);
            }
        }
        const auto cs = getSizeContent();
        int32_t height = theme->get(GuiConstant::CONST_ROW_HEIGHT);

        auto inset = math::max<int32_t>(5, height / 2);
        int x  = inset;
        int y  = inset;
        int x2 = cs.x - inset;

        auto printL = [&](int n, const char* caption, const String& str) {
            float offsetX = (n) * inset + x;
            renderText(vg, vec2(offsetX, y), vec2(cs.x - inset*2, height), caption, height, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
            renderText(vg, vec2(x2, y), vec2(cs.x - inset*2, height), str, height, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
            y += height;
        };


        if (stats.usageRaw >= 1.0) {
            nvgFillColor(vg, theme->getColor(GuiColor::COL_LEVEL_IND_YELLOW_DRKER));
        } else {
            nvgFillColor(vg, THEMECOL_TEXT);
        }
        auto& renderStats = daw_tls::getTls().runtime->prevRenderStats;
        printL(0, "Usage", StringFormat("%.2f%% (%.2f%%)", stats.usage * 100.0f, stats.usageRaw * 100.0f));
        printL(0, "playThreadLockCount (frame)", StringFormat("%zd", renderStats.playThreadLockCount));
        nvgFillColor(vg, THEMECOL_TEXT);
        y += height / 2;

        if (stream) {
            auto meterPos = ivec2{inset, y};
            auto meterSize = ivec2{cs.x - inset, theme->get(GuiConstant::CONST_METER_WIDTH)};
            auto metersInputHost = host->getMeterInput();
            auto metersOutputHost = host->getMeterOutput();
            auto meterCallbackInput = stream->getMeterCallbackInput();
            auto meterCallbackOutput = stream->getMeterCallbackOutput();
            if (meterCallbackInput) {
                printL(0, "Audio Callback Input", StringFormat("%.3f", meterCallbackInput->getMaxRMS()));
                renderMeterHorizontal(vg, theme, ivec2{inset, y}, meterSize, meterCallbackInput, &label);
                y += meterSize.y + 5;
            }
            if (stream->getMeterInput().getNumChannels() > 1) {
                auto metersInput = stream->getMeterInput().getSubChannelMeter(0, 2);
                printL(0, "Audio Stream Input", StringFormat("%.3f", metersInput.getMaxRMS()));
                renderMeterHorizontal(vg, theme, ivec2{inset, y}, meterSize, &metersInput, &label);
                y += meterSize.y + 5;
            }
            if (metersInputHost) {
                auto subMeter = metersInputHost->getSubChannelMeter(0, 2);
                printL(0, "Host Input", StringFormat("%.3f", subMeter.getMaxRMS()));
                renderMeterHorizontal(vg, theme, ivec2{inset, y}, meterSize, &subMeter, &label);
                y += meterSize.y + 5;
            }
            if (metersOutputHost) {
                auto subMeter = metersOutputHost->getSubChannelMeter(0, 2);
                printL(0, "Host Output", StringFormat("%.3f", subMeter.getMaxRMS()));
                renderMeterHorizontal(vg, theme, ivec2{inset, y}, meterSize, &subMeter, &label);
                y += meterSize.y + 5;
            }
            if (stream->getMeterOutput().getNumChannels() > 1) {
                auto metersOutput = stream->getMeterOutput().getSubChannelMeter(0, 2);
                printL(0, "Audio Stream Output", StringFormat("%.3f", metersOutput.getMaxRMS()));
                renderMeterHorizontal(vg, theme, ivec2{inset, y}, meterSize, &metersOutput, &label);
                y += meterSize.y + 5;
            }
            if (meterCallbackOutput) {
                printL(0, "Audio Callback Output", StringFormat("%.3f", meterCallbackOutput->getMaxRMS()));
                renderMeterHorizontal(vg, theme, ivec2{inset, y}, meterSize, meterCallbackOutput, &label);
                y += meterSize.y + 5;
            }
        }
        y += height / 2;
        
        String strOutputTickPos = tickAsBeatString(math::rounddS32(host->getOutputTickPos()), false);
        printL(0, "audioCallback tickPos", StringAsCStr(strOutputTickPos));
        if (stream) {
            auto timings = stream->getStreamTimings();
            printL(0, "audioCallback tDelta", StringFormat("%.1fµs min %.1fµs max %.1fµs avg", timings.tmDeltaCbMin / 1000.0, timings.tmDeltaCbMax / 1000.0, timings.tmDeltaCbAvg / 1000.0));
            printL(0, "audioCallback samplePos", StringFormat("%zd", timings.samplePos));
            printL(0, "audioCallback samplePosProcIn", StringFormat("%zd", timings.samplePosProcIn));
            printL(0, "audioCallback samplePosProcOut", StringFormat("%zd", timings.samplePosProcOut));
            printL(0, "outputBufferUnderuns", StringFormat("%u", stream->bufferUnderuns));
            printL(0, "inputBufferUnderuns", StringFormat("%u", stream->inputBufferUnderuns));
            printL(0, "stream input time", StringFormat("%f", stream->inputTimeSeconds));
            printL(0, "stream output time", StringFormat("%f", stream->outputTimeSeconds));
            printL(0, "d time", StringFormat("%f", stream->inputTimeSeconds-stream->outputTimeSeconds));
            printL(0, "stream input pos", StringFormat("%zd", stream->inputSamplePos));
            printL(0, "stream output pos", StringFormat("%zd", stream->outputSamplePos));
            printL(0, "d pos", StringFormat("%zd", stream->inputSamplePos-stream->outputSamplePos));
        }

        y += height / 2;
        printL(0, "input q len", StringFormat("%d", stats.inputQueueLen));
        printL(0, "output q len", StringFormat("%d", stats.outputQueueLen));
        printL(0, "INPUT  resampler", StringFormat("%d samples|%d blocks", stats.resamplerInNumSamples, stats.resamplerInNumBlocks));
        printL(0, "OUTPUT resampler", StringFormat("%d samples|%d blocks", stats.resamplerOutNumSamples, stats.resamplerOutNumBlocks));
        printL(0, "Resampler delay IN/OUT", StringFormat("%zd/%zd samples", stats.resamplerDelayInput, stats.resamplerDelayOutput));
        printL(0, "output q len", StringFormat("%d", stats.outputQueueLen));

        y += height / 2;
        printL(0, "Blocks Processed", StringFormat("%d", stats.blocksProcessed));
        printL(0, "Samples Processed", StringFormat("%d", stats.samplesProcessed));
        printL(0, "All Plugins", StringFormat("%zd µs (%zd µs)", stats.timeProcessPlugins, stats.timeProcessPluginsRaw));
        printL(0, "Block", StringFormat("%zd µs (%zd µs)", stats.timeBlock, stats.timeBlockRaw));
        std::vector<String> keyset;
        keyset.reserve(stats.timings.size());
        for (auto& entry : stats.timings) {
            insertSorted(keyset, String(entry.first));
        }
        for (const String& entryKey : keyset) {
            const auto entryVal = [&entryKey, &timingMap = stats.timings]() -> int64_t { 
                for (auto& mapEntry : timingMap) {
                    if (entryKey == mapEntry.first)
                        return mapEntry.second;
                }
                return 0;
            }();

            int ident    = 0;
            int iLeftCut = 0;
            for (auto& c : entryKey) {
                if ('.' == c) {
                    iLeftCut = (&c) - &entryKey.front() + 1;
                    ident++;
                }
            }

            String format = "%zd µs";
            if (entryKey.find("Bytes") != String::npos) {
                format = "%zd bytes";
            }
            if (entryKey.find("SSE") != String::npos) {
                format = "%08X";
            }


            const String label = entryKey.substr(iLeftCut);
            printL(ident, StringAsCStr(label), StringFormat(StringAsCStr(format), entryVal));
            if (ident && StrEndsWith(label, "ProcessMidi")) {
                ident++;
                printL(ident, "InputClips", StringFormat("%zd µs", stats.blockMidiStats.tm0InputClips));
                printL(ident, "InputRealtime", StringFormat("%zd µs", stats.blockMidiStats.tm1InputRT));
                printL(ident, "ProcessNotes", StringFormat("%zd µs", stats.blockMidiStats.tm2ProcNotes));
                printL(ident, "RevalidateEnds", StringFormat("%zd µs", stats.blockMidiStats.tm3RevalidateEnds));
                printL(ident, "SortEvents", StringFormat("%zd µs", stats.blockMidiStats.tm4SortEvents));
                printL(ident, "ProcArp", StringFormat("%zd µs", stats.blockMidiStats.tm5ProcArp));
                printL(ident, "OuputPostUpdate", StringFormat("%zd µs", stats.blockMidiStats.tm6UpdateOutputPost));
                printL(ident, "ValidateMidi", StringFormat("%zd µs", stats.blockMidiStats.tm7ValidateMidi));
            }
        }
        y += height / 2;

        prof_stats_window_t profDataWindow;
        if (ProfilingImpl::profilingGetRecentFrame(dawCtrl->window, &profDataWindow)) {
            printL(0, "App Tick", StringFormat("%zd µs", profDataWindow.timeAppTick));
            printL(0, "Render", StringFormat("%zd µs", profDataWindow.timeRender));
        }
        printL(1, "Prerender", StringFormat("%zd µs", renderStats.timePrerender));
        printL(1, "UpdateWaveforms", StringFormat("%zd µs", renderStats.timeUpdateWaveforms));
        printL(1, "RenderEditor", StringFormat("%zd µs", renderStats.timeRenderEditor));
        printL(1, "RenderTrackControls", StringFormat("%zd µs", renderStats.timeRenderTrackControls));
        printL(0, "Clips in view", StringFormat("%zd", renderStats.clipsRendered));
        printL(0, "Notes in view", StringFormat("%zd", renderStats.notesRendered));
        y += height / 2;
        {
            const char* sufArr[3] = { "B", "KB", "MB" };
            size_t clipSufIdx     = 0;
            int64_t clipCacheSize        = daw_tls::getTls().runtime->renderClipCacheStats.sizeCacheAllocatedMemBytes;
            double clipCacheSizeAsDouble = clipCacheSize;
            while (clipCacheSizeAsDouble >= 1024.0 && clipSufIdx < 2) {
                clipCacheSizeAsDouble /= 1024.0;
                clipSufIdx++;
            }
            printL(0, "clip_render_cache size", StringFormat("%f %s", clipCacheSizeAsDouble, sufArr[clipSufIdx % 3]));
        }

        minHTop = y + height;
    }
    void determineSize(ivec2& prefSize) override {
        prefSize.x = math::max(100, prefSize.x);
        prefSize.y = math::max(math::max(minHTop, 100), prefSize.y);
    }
};

class gui_performance final : public guictr_base {
public:
    gui_performance_stats textStats;
    guictr_scrollbar scrollTop;
    gui_performance() : guictr_base() {
        setCanMouseHit(true);
        setGuiType(CTR_TYPE_PERFORMANCE);
        padding = 4;
        margin = 2;

        textStats.padding = 0;
        textStats.setBackgroundRendered(false);
        scrollTop.padding = 0;
        scrollTop.setBackgroundRendered(true);
        scrollTop.add(&textStats);
        scrollTop.maxHeight = -1;
        add(&scrollTop);
    }
    ~gui_performance() override {
        removeGuis();
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
        layout();
    }
    void layout() override {
        ivec2 cs       = getSizeContent();
        scrollTop.pos  = ivec2(0, 0);
        scrollTop.size = ivec2(cs.x, cs.y);
        scrollTop.maxHeight = cs.y;
        scrollTop.determineSize(scrollTop.size);
        for (guibase* gui : guis) {
            gui->layout();
        }
    }
    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        if (!setScissorTransform(vg)) {
            return;
        }
        for (auto* g : guis) {
            nvgSave(vg);
            g->render(vg);
            nvgRestore(vg);
        }
    }
};

namespace DAW::UI {
    guictr_base* makeGuiPerformance(create_ctr_t ctxt) {
        return new gui_performance();
    }
}
