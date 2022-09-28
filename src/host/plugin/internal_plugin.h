#pragma once
#include <vector>
#include <memory>

#include "automation.h"
#include "base_plugin.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "logging.h"
#include "math/vec.h"
#include "meter.h"
#include "platform.h"
#include "plugins/plugincontrol.h"
#include "plugins/synth/IPlugMidi.h"
#include "seq_time.h"
#include "snapshot.h"
#include "str_util.h"
#include "types.h"
#include "window.h"

class guiplugin;
class PluginViewContainers;
class track_t;
struct AudioBlock;
struct handles_t;
struct track_impl_t;


class internalplugin : public effectbase {
public:
    struct internalplugin_handles_t {
        std::unique_ptr<guiinternalpluginview> gui;
        std::vector<int32_t> heldNotes;
    };
protected:
    struct internal_plugin_window_client {
        std::shared_ptr<PluginViewContainers> view;
        std::shared_ptr<PluginControl> ctrl;
        host_plugin_window* hostWindow = nullptr;
        window_main* clientWindow = nullptr;
        window_plugin* clientWindowInterface = nullptr;
    };
    internalplugin_handles_t* handlesIntPlugin;
    virtual std::shared_ptr<PluginViewContainers> createViewCtrInternal() { return nullptr; };
public:
    String sDir;
    int32_t pluginCategory = 0;
    int32_t vstVersion     = 0;
    uint32_t uId            = 0;
    internal_plugin_window_client windowClient;

    internalplugin(String _sName, int32_t _pluginType, int32_t _projectGlobalId, IHostCallback* _hostCallback);
    ~internalplugin() override;

    samplecount_t getPluginLatency() override { return 0; };

    guiplugin* makeGui() override;
    guiplugin* getGui() override;
    bool onShow(host_plugin_window* _window) override;
    bool onClose() override;
    void updateWindow() override;
    bool hasWindowEditor() override {
        return true;
    }
    void onWindowResize(ivec2 size) override;
    bool showWindow(bool bResetPosition) override;

    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    
    virtual std::shared_ptr<std::vector<std::byte>> storePresetData() { return nullptr; };
    virtual bool loadPresetData(const std::shared_ptr<std::vector<std::byte>>& buf) { return false; };

    // automatable_t interface
    String getAutomatableName() override;
    float getParamValue(int32_t idx) override;
    void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override { out->copyFrom(in); };
    void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
    virtual void processMidiMessages(std::vector<IMidiMsg>& midiEvents) { };
    void processMidi(midi_events_t& midiEvents) override;
    void sendNotesOff() override;
    std::shared_ptr<PluginViewContainers> getViewCtr(int32_t uiId);
};

class internal_automator : public internalplugin {
public:
    internal_automator(String _sName, int32_t _pluginType, int32_t _projectGlobalId, IHostCallback* _hostCallback)
    : internalplugin(_sName, _pluginType, _projectGlobalId, _hostCallback) {
    }
    ~internal_automator() override {
    }
    virtual const automated_param_t* getModulationOutputData(int32_t channel) const = 0;
    DAW::automation_channel_ref getModulationChannel(int32_t channel) const {
        auto thisRef = toRef();
        thisRef.paramIdx = channel;
        thisRef.type = AUTOMATABLE_MODULATION_SRC;
        return DAW::automation_channel_ref{ 0, thisRef };
    }
};
