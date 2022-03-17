#include <algorithm>
#include "util/profiling.h"
#include "util/profiling_impl.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "renderresources.h"
#include "gui/gui.h"
#include "gui/container/container.h"
#include "gui/container/scrollcontainer.h"
#include "gui/controls/textfield.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"
#include "host/audio_host.h"
#include "appconfig.h"

class gui_performance_stats : public guictr_base {
    int32_t minHTop = 66;
    host_stats_t stats{ 0 };
    int64_t timeLastUpdate = 0L;
    playback_state state{ status_stop };

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
        if (getTimeMicros() - timeLastUpdate >= 250000) {
            timeLastUpdate  = getTimeMicros();
            ThreadLock lock = daw->getPlayThread()->tryLockThread();
            if (lock.isLocked()) {
                state = daw->getPlayThread()->getState();
                host->getStats(stats);
            }
        }
        //const int fontSize = 12;
        int32_t height = theme->get(GuiConstant::CONST_ROW_HEIGHT);

        auto inset = math::max<int32_t>(5, height / 2);
        int x  = inset;
        int y  = inset;
        int x2 = getSizeContent().x - inset;

        auto printL = [&](int inset, const char* caption, const String& str) {
            float offsetX = (inset + 1) * x;
            renderText(vg, vec2(offsetX, y), vec2(size.x - inset*2, height), caption, height, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
            renderText(vg, vec2(x2, y), vec2(size.x - inset*2, height), str, height, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
            y += height;
        };

        if (stats.usageRaw >= 1.0) {
            nvgFillColor(vg, theme->getColor(GuiColor::COL_LEVEL_IND_YELLOW_DRKER));
        } else {
            nvgFillColor(vg, THEMECOL_TEXT);
        }
        auto& renderStats = daw_tls::getTls().runtime->prevRenderStats;
        printL(0, "Usage", StringFormat("%.2f%% (%.2f%%)", stats.usage * 100.0f, stats.usageRaw * 100.0f));
        nvgFillColor(vg, THEMECOL_TEXT);
        
        prof_stats_window_t profDataWindow;
        if (ProfilingImpl::profilingGetRecentFrame(dawCtrl->window, &profDataWindow)) {
            printL(0, "Render", StringFormat("%d µs", profDataWindow.timeRender));
        }
        printL(1, "Prerender", StringFormat("%d µs", renderStats.timePrerender));
        printL(1, "UpdateWaveforms", StringFormat("%d µs", renderStats.timeUpdateWaveforms));
        printL(1, "RenderEditor", StringFormat("%d µs", renderStats.timeRenderEditor));
        printL(1, "RenderTrackControls", StringFormat("%d µs", renderStats.timeRenderTrackControls));
        printL(0, "Clips in view", StringFormat("%d", renderStats.clipsRendered));
        printL(0, "Notes in view", StringFormat("%d", renderStats.notesRendered));
        y += height / 2;

        printL(0, "Blocks Processed", StringFormat("%d", stats.blocksProcessed));
        printL(0, "Samples Processed", StringFormat("%d", stats.samplesProcessed));
        printL(0, "All Plugins", StringFormat("%lld µs (%lld µs)", stats.timeProcessPlugins, stats.timeProcessPluginsRaw));
        printL(0, "Block", StringFormat("%lld µs (%lld µs)", stats.timeBlock, stats.timeBlockRaw));
        for (auto& entry : stats.timings) {
            const String& entryKey = entry.first;

            int ident    = 0;
            int iLeftCut = 0;
            for (auto& c : entryKey) {
                if ('.' == c) {
                    iLeftCut = (&c) - &entryKey.front() + 1;
                    ident++;
                }
            }

            String format = "%lld µs";
            if (entryKey.find("Bytes") != String::npos) {
                format = "%lld bytes";
            }
            if (entryKey.find("SSE") != String::npos) {
                format = "%08X";
            }


            const String label = entryKey.substr(iLeftCut);
            printL(ident, StringAsCStr(label), StringFormat(StringAsCStr(format), entry.second));
            if (ident && label == "ProcessMidi") {
                ident++;
                printL(ident, "InputClips", StringFormat("%lld µs", stats.blockMidiStats.tm0InputClips));
                printL(ident, "InputRealtime", StringFormat("%lld µs", stats.blockMidiStats.tm1InputRT));
                printL(ident, "ProcessNotes", StringFormat("%lld µs", stats.blockMidiStats.tm2ProcNotes));
                printL(ident, "RevalidateEnds", StringFormat("%lld µs", stats.blockMidiStats.tm3RevalidateEnds));
                printL(ident, "SortEvents", StringFormat("%lld µs", stats.blockMidiStats.tm4SortEvents));
                printL(ident, "ProcArp", StringFormat("%lld µs", stats.blockMidiStats.tm5ProcArp));
                printL(ident, "WriteVstEvents", StringFormat("%lld µs", stats.blockMidiStats.tm6WriteVstEvents));
                printL(ident, "ProcessOutput", StringFormat("%lld µs", stats.blockMidiStats.tm7ProcessOutput));
            }
        }
        y += height / 2;
        printL(0, "audioCallback tDelta", StringFormat("%d µs", audioHost ? audioHost->audioCallbackInvocationDelay_usec : 0));
        printL(0, "outputBufferUnderuns", StringFormat("%u", audioHost ? audioHost->bufferUnderuns : 0));
        printL(0, "inputBufferUnderuns", StringFormat("%u", audioHost ? audioHost->inputBufferUnderuns : 0));
        printL(0, "input q len", StringFormat("%d", stats.inputQueueLen));
        printL(0, "output q len", StringFormat("%d", stats.outputQueueLen));
        printL(0, "INPUT  resampler", StringFormat("%d samples|%d blocks", stats.resamplerInNumSamples, stats.resamplerInNumBlocks));
        printL(0, "OUTPUT resampler", StringFormat("%d samples|%d blocks", stats.resamplerOutNumSamples, stats.resamplerOutNumBlocks));
        printL(0, "output q len", StringFormat("%d", stats.outputQueueLen));

        printL(0, "playThreadLockCount (frame)", StringFormat("%d", renderStats.playThreadLockCount));
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

class gui_performance : public guictr_base {
public:
    gui_performance_stats textStats;
    guictr_scrollbar scrollTop;
    gui_performance() : guictr_base() {
        ctrType = CTR_TYPE_PERFORMANCE;
        setBackgroundRendered(false);
        padding = 0;
        margin  = 0;

        textStats.padding = 0;
        textStats.setBackgroundRendered(false);
        scrollTop.padding = 0;
        scrollTop.setBackgroundRendered(false);
        scrollTop.add(&textStats);
        scrollTop.maxHeight = -1;
        add(&scrollTop);
    }
    ~gui_performance() override {
        removeGuis();
    }
    void onTick(AppCtrl* ctrl) override {
        guictr_base::onTick(ctrl);
    }
    void layout() override {
        ivec2 cs       = getSizeContent();
        scrollTop.pos  = ivec2(0, 0);
        scrollTop.size = ivec2(cs.x, cs.y);
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

guictr_base* makeGuiPerformance() {
    return new gui_performance();
}
