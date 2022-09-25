#include "appsettings.h"
#include "assert_dbg.h"
#include "audio_config.h"
#include "audioblock.h"
#include "audiobuffer.h"
#include "audiocache.h"
#include "automation.h"
#include "basectrl.h"
#include "clip.h"
#include "cursor.h"
#include "dsp_util.h"
#include "effect_graph.h"
#include "exceptions.h"
#include "fileio.h"
#include "gui/container/container.h"
#include "gui/controls/button.h"
#include "gui/gui.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginctr.h"
#include "gui/track/subtrack.h"
#include "gui/track/trackcontent.h"
#include "gui/track/trackcontrols.h"
#include "gui/track/trackctr.h"
#include "history.h"
#include "host/audio_config.h"
#include "host/daw_channel.h"
#include "host/history.h"
#include "host/host_plugin_window.h"
#include "host/mainctrl.h"
#include "host/plugin/vst_plugin.h"
#include "host/pluginmanager.h"
#include "host/vst_event.h"
#include "host_plugin_window.h"
#include "logging.h"
#include "mainctrl.h"
#include "math/seq_math.h"
#include "meter.h"
#include "midi-defs.h"
#include "midi-msg.h"
#include "midi_host.h"
#include "midiarp.h"
#include "modules.h"
#include "note.h"
#include "platform.h"
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"
#include "plugindatabase.h"
#include "host/pluginmanager.h"
#include "plugins/gain/gain-plugin.h"
#include "plugins/info/info-plugin.h"
#include "plugins/latency/latency-plugin.h"
#include "plugins/samplecrush/samplecrush-plugin.h"
#include "plugins/sampledelay/sampledelay-plugin.h"
#include "plugins/stereowidth/stereowidth-plugin.h"
#include "plugins/synth/synth-plugin.h"
#include "project.h"
#include "projectcontroller.h"
#include "resampler.h"
#include "saferef.h"
#include "samplerate.h"
#include "seq_time.h"
#include "seq_util.h"
#include "snapshot.h"
#include "sse.h"
#include "str_util.h"
#include "thread.h"
#include "threads/childprocessthread.h"
#include "threads/threadlock.h"
#include "threads/workerthread.h"
#include "tls.h"
#include "track.h"
#include "track_graph.h"
#include "track_impl.h"
#include "types.h"
#include "util/profiling.h"
#include "pluginmanager.h"
#include "wave/waveform_render_impl.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <dlfcn.h>
#include <dr_libs/dr_wav.h>
#include <emmintrin.h>
#include <memory.h>
#include <memory>
#include <memory>
#include <utility>
#include <vector>
#include <vstsdk-host-2.4/aeffectx.h>

namespace DAW {

class pluginmanager::ModuleManager {
public:
    ModuleManager() = default;

    void releaseModule(void* module) {
#ifdef _WIN32
        String moduleName = getModuleName((HMODULE)module);
        log_printf("Unload %s\n", StringAsCStr(moduleName));
        FreeLibrary((HMODULE)module);
#endif
#if defined(__linux__) || defined(__APPLE__)
        dlclose(module);
#endif
    }
};


pluginmanager::pluginmanager() noexcept
    : mgrImpl(new pluginmanager::pluginmanager_impl()), moduleMgr{new pluginmanager::ModuleManager{}} 
{
    registerModules();
}

pluginmanager::~pluginmanager() {
    delete moduleMgr;
}

void DAW::plugin_host_callback::onUiChanged(effectbase* effect) {
    if (effect) {
        // NOTE: this loop might kill performance
        effect->visitParams([](auto& mapEntry) {
            automatable_param_t& param = mapEntry.second;
            param.paramValueState |= plugin_param_sync_state::PARAM_FLAG_DIRTY;
            param.paramDisplayValState |= PARAM_FLAG_DIRTY;
        });
    }
}

void pluginmanager::setTls(daw_tls::tlsinstance& tls) {
    this->mgrImpl->tls = tls;
}

void pluginmanager::unloadPlugin(effectbase* plugin, int flags) {
    bool notifyUp = !(flags & FLAG_HOST_UNLOAD_PLUGIN_NO_NOTIFY);
    if (notifyUp) {
        //TODO: this shouldn't be here!
        if (MainCtrl::get())
            MainCtrl::get()->closeContextMenu();
    }

    plugin->onPreUnload(flags);
    audio_stage_t* audioStage = plugin->getTrackLink();
    if (audioStage) {
        audioStage->removePlugin(plugin, false);
        if (notifyUp) {
            audioStage->pluginsChanged();

        }
    }

    if (notifyUp) {
        plugin->closeWindow();
    }
    plugin->unload(this, flags);

    switch (plugin->getModuleType()) {
    case PLUGIN_TYPE_DEFERRED:
        always_assert(removeEntry(pluginsDeferred, plugin));
        break;
    case PLUGIN_TYPE_INTERNAL_EFFECT:
    case PLUGIN_TYPE_VST:
        always_assert(removeEntry(pluginInstancesVST2, plugin));
        always_assert(removeEntry(pluginInstances, plugin));
        break;
    default:
        always_assert(removeEntry(pluginInstancesInternal, plugin));
        always_assert(removeEntry(pluginInstances, plugin));
        break;
    }

    //PopupCtrl::get()->close(); // Make sure context controls do not reference vst
    if (plugin->getModuleType() == PLUGIN_TYPE_VST || plugin->getModuleType() == PLUGIN_TYPE_INTERNAL_EFFECT) {
        vstplugin* vst = dynamic_cast<vstplugin*>(plugin);
        if (vst->internalModuleId <= 0) {
            moduleMgr->releaseModule(vst->handle->hmodule);
        }
    }
    delete plugin;
    if (notifyUp) {
        if (DawInstance::get()) DawInstance::get()->onPluginsChanged();
    }
    dbgassert(validateIds());
}

} // namespace DAW
