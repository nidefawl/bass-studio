#pragma once
#ifdef __APPLE__
#endif
#include "types.h"
#include "math/vec.h"
#include "str_util.h"
#include "seq_time.h"

#include "automation.h"
#include "logging.h"
#include "platform.h"
#include "meter.h"
#include "snapshot/snapshot.h"
#include "base_plugin.h"

struct AudioBlock;
struct handles_t;
class host_plugin_window;

class auplugin : public effectbase {
public:
    handles_t* const handle;
    String sDir;
    bool bInEditIdle   = false;
    int pluginCategory = 0;
    int vstVersion     = 0;
    int uId            = 0;
    host_plugin_window* window = NULL;
    std::vector<String> programNames;
    std::vector<String> inputNames;
    std::vector<String> outputNames;
    auplugin(handles_t* _handle, int32_t globalId, IHostCallback* hostcallback, String sDir, String sName)
        : effectbase(sName, PLUGIN_TYPE_AU, globalId, hostcallback), handle(_handle) {
        this->sDir = sDir;
    }
    ~auplugin() override = default;

protected:
    void onEnable() override;
    void onDisable() override;

public:
    int getModuleType() override { return PLUGIN_TYPE_AU; };

    bool hasWindowEditor() override {
        return false;
    }
    void unload(DAW::Host::PluginManager* host, int flags) override;
    void load(DAW::Host::PluginManager* host) override;

    // automatable_t interface
    String getAutomatableName() override;
    float getParamValue(int32_t idx) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
    automatable_param_ref_t toRef() const override;

    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_snapshot_t& pluginSnapshot) override;
    guiplugin* makeGui() override;
    guiplugin* getGui() override;
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    samplecount_t getPluginLatency() override;
};
