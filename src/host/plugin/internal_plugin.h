#pragma once
#include <vector>
#include <memory>

#include "automation.h"
#include "base_plugin.h"
#include "gui/plugin/pluginviewcontainers.h"
#include "logging.h"
#include "math/vec.h"
#include "meter.h"
#include "platform.h"
#include "plugins/plugincontrol.h"
#include "plugins/plugincontrol.h"
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
protected:
    struct internal_plugin_window_client {
        std::shared_ptr<PluginViewContainers> view;
        std::shared_ptr<PluginControl> ctrl;
        host_plugin_window* hostWindow = nullptr;
        window_main* clientWindow = nullptr;
        window_plugin* clientWindowInterface = nullptr;
    };
    struct internalplugin_handles_t;
    internalplugin_handles_t* handlesIntPlugin;

public:
    std::vector<std::shared_ptr<PluginViewContainers>> views;
    String sDir;
    int32_t pluginCategory = 0;
    int32_t vstVersion     = 0;
    uint32_t uId            = 0;
    internal_plugin_window_client windowClient;

    internalplugin(String _sName, int32_t _pluginType, int32_t _projectGlobalId);
    ~internalplugin() override;

    virtual float dispatchGetParameter(int32_t idx)           = 0;
    virtual void dispatchSetParameter(int32_t idx, float val) = 0;

    samplecount_t getPluginLatency() override = 0;
    void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override = 0;

    guiplugin* makeGui() override;
    guiplugin* getGui() override;
    virtual std::shared_ptr<PluginViewContainers> createInternalView() {
        return nullptr;
    };

    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
    // automatable_t interface
    String getAutomatableName() override;
    float getParamValue(int32_t idx) override;
    void setParamValue(int32_t idx, float val, int flags) override;
    automationlane_snapshot_t toRef() const override;

    bool onShow(host_plugin_window* _window) override;
    bool onClose() override;
    void updateWindow() override;
    bool hasWindowEditor() override {
        return true;
    }
    void onWindowResize(ivec2 size) override;
    bool showWindow(bool bResetPosition) override;
};
