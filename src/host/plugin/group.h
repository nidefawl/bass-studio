#pragma once

#include "modules.h"
#include "dsp_util.h"
#include "internal_plugin.h"
#include "str_util.h"
#include "snapshot.h"

struct audio_stage_t;
class guiplugin;
class guibase;
class vsthost;
namespace DAW {
    struct processing_graph_t;
    using effect_processing_graph_t  = processing_graph_t;
}
class module_group : public internalplugin {
    struct internal_handles_t;
    internal_handles_t* handle;
    audio_stage_t* audio;
    std::shared_ptr<DAW::effect_processing_graph_t> lastEffProcessingGraph;
public:
    explicit module_group(int32_t _projectGlobalId);
    ~module_group() override;
    float dispatchGetParameter(int32_t idx) override;
    void dispatchSetParameter(int32_t idx, float val) override;

public:
    int getModuleType() override { return PLUGIN_TYPE_GROUP; };
    guiplugin* makeGui() override;
    guiplugin* getGui() override;
    samplecount_t getPluginLatency() override;
    void process(AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    void processMidi(midi_events_t& midiEvents) override;
    String getInfo(std::vector<String>& list) override;
    void onEnable() override;
    void onDisable() override;
    void unload(vsthost* host, int flags) override;
    void onPreUnload(int flags) override;
    void load(vsthost* host) override;
    void breakTrackLink() override;
    void setTrackLink(audio_stage_t* trImpl) override;
    audio_stage_t* getAudioStage() { return audio; };
    void onTick(double since) override;
    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_snapshot_t& snapshot) override;
    void postProcess(AudioBlock* out, int32_t samples, bool hasProcessed) override;
    void getChildAudioStages(std::vector<audio_stage_t*>& targets) override;
    void getDeferredEffects(std::vector<effectbase*>& effects) override;
    std::shared_ptr<DAW::effect_processing_graph_t> getLastProcessingGraph();
};
