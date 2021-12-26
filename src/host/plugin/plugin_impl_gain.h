#pragma once
#include "str_util.h"
#include "modules.h"
#include "internal_plugin.h"
#include "host/vst_host.h"
#include "track_impl.h"

class guiplugin;
class vsthost;
struct audio_stage_t;

class module_gain : public internalplugin {
    struct internal_handles_t;
    internal_handles_t* handle;

public:
    explicit module_gain(int32_t _projectGlobalId);
    ~module_gain() override;
    float dispatchGetParameter(int32_t idx) override;
    void dispatchSetParameter(int32_t idx, float val) override;

public:
    int getModuleType() override { return PLUGIN_TYPE_GAIN; };
    int32_t getPluginLatency() override;
    void process(AudioBlock* in, AudioBlock* out, double tick, int32_t samplePos, int32_t numSamples, playback_state state) override;
    String getInfo(std::vector<String>& list) override;
    void resume() override;
    void sleep() override;
    void unload(vsthost* host, int flags) override;
    void onPreUnload(int flags) override;
    void load(vsthost* host) override;
    void breakTrackLink() override;
    void setTrackLink(audio_stage_t* trImpl) override;
    void onTick(double since) override;
    void makeSnapshot(plugin_snapshot_t& ps, bool storePluginChunks) override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
    std::shared_ptr<PluginViewContainers> createInternalView() override;
    String formatDisplayValue(int32_t idx) override;
};
