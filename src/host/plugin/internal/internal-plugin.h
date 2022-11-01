#pragma once
#include <optional>
#include <vector>
#include <memory>

#include "host/automation/automation.h"
#include "host/plugin/base/base-plugin.h"
#include "gui/plugin/plugin.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "logging.h"
#include "math/vec.h"
#include "host/meter/meter.h"
#include "platform.h"
#include "plugins/plugincontrol.h"
#include "plugins/synth/IPlugMidi.h"
#include "seq_time.h"
#include "snapshot/snapshot.h"
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
    void updateFromMainThread() override;
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
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override { out->copyFrom(in); };
    void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
    
    void getAllViewCtrs(int32_t uiId, std::vector<std::shared_ptr<PluginViewContainers>>& vec);
    std::shared_ptr<PluginViewContainers> openViewCtr(int32_t uiId);
};

class internal_modulator : public internalplugin {
protected:
    std::vector<DAW::modulation_channel_desc> outputModChannelsDesc;
public:
    internal_modulator(String _sName, int32_t _pluginType, int32_t _projectGlobalId, IHostCallback* _hostCallback)
    : internalplugin(_sName, _pluginType, _projectGlobalId, _hostCallback) {
    }
    ~internal_modulator() override {
    }
    virtual void initModChannels() = 0;
    virtual const automated_param_t* getModulationOutputData(const DAW::modulation_channel_ref& modChannel) = 0;
    DAW::modulation_channel_ref getModulationChannel(int32_t channel) const {
        auto thisRef = toRef();
        thisRef.paramIdx = channel;
        thisRef.type = AUTOMATABLE_MODULATOR_OUTPUT;
        return DAW::modulation_channel_ref{ 0, thisRef, {} };
    }
    bool hasAutomationModulationOutput() const override {
        return true;
    }
    const std::vector<DAW::modulation_channel_desc>& getModulationOutputChannelDesc() const {
        return outputModChannelsDesc;
    }
};
