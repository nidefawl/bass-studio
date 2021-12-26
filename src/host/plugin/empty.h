#pragma once
#include <vector>
#include "internal_plugin.h"
#include "modules.h"
#include "str_util.h"

class guiplugin;
class vsthost;
struct audio_stage_t;
class module_empty : public internalplugin {
    struct internal_handles_t;
    internal_handles_t* handle;

public:
    explicit module_empty(int32_t _projectGlobalId);
    ~module_empty() override;
    float dispatchGetParameter(int32_t idx) override;
    void dispatchSetParameter(int32_t idx, float val) override;

public:
    int getModuleType() override { return PLUGIN_TYPE_EMPTY; };
    guiplugin* makeGui() override;
    guiplugin* getGui() override;
    int32_t getPluginLatency() override;
    void process(AudioBlock* in, AudioBlock* out, double tick, int32_t samplePos, int32_t numSamples, playback_state state) override;
    String getInfo(std::vector<String>& list) override;
    void resume() override;
    void sleep() override;
    void unload(vsthost* host, int flags) override;
    void load(vsthost* host) override;
    void breakTrackLink() override {
        internalplugin::breakTrackLink();
    }
    void setTrackLink(audio_stage_t* trImpl) override {
        internalplugin::setTrackLink(trImpl);
    }
    bool isBypass() override {
        return true;
    }
};
