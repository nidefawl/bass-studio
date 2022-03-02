#pragma once

#include <utility>

#include "math/vec.h"
#include "str_util.h"
#include "seq_time.h"

#include "automation.h"
#include "logging.h"
#include "platform.h"
#include "meter.h"
#include "snapshot.h"
#include "base_plugin.h"

class vsthost;
struct AudioBlock;
struct handles_t;
class track_t;
class guibase;
struct track_impl_t;
class vst_window;
struct VstTimeInfo;

/** Flags used in VstParameterProperties. */
enum vst_param_flags {
    ParamIsSwitch                = 1 << 0,///< parameter is a switch (on/off)
    ParamUsesIntegerMinMax       = 1 << 1,///< minInteger, maxInteger valid
    ParamUsesFloatStep           = 1 << 2,///< stepFloat, smallStepFloat, largeStepFloat valid
    ParamUsesIntStep             = 1 << 3,///< stepInteger, largeStepInteger valid
    ParamSupportsDisplayIndex    = 1 << 4,///< displayIndex valid
    ParamSupportsDisplayCategory = 1 << 5,///< category, etc. valid
    ParamCanRamp                 = 1 << 6,///< set if parameter value can ramp up/down
    ParamIsAdvanced              = 1 << 31///< set if parameter value can ramp up/down
};

struct vst_param_category {
    int32_t idx;
    int16_t nParams;
    String label;//24
};
enum vst_param_state : uint8_t {
    PARAM_FLAG_DIRTY = 1,
    PARAM_FLAG_SET = 2
};

class vstplugin : public effectbase {
public:
    handles_t* const handle;
    /** -1 for external, >= 0 for internal plugins */
    const int32_t internalModuleId;
    String sDir;
    bool bInEditIdle       = false;
    bool bWantsEffIdle     = false;
    bool bIsLoadingProgram = false;
    bool bIsPostInit       = false;
    int32_t pluginCategory = 0;
    int32_t vstVersion     = 0;
    int32_t localDbId      = -1;
    int32_t vendorVersion  = 0;
    uint32_t uId           = 0;
    vst_window* window = nullptr;
    bool isInSuspend   = true;
    std::vector<vst_param_category> paramsCategories;

    //TODO: this is not thread safe
    std::map<int32_t, vst_opcode_stats_t> opCodeIn;
    std::map<int32_t, vst_opcode_stats_t> opCodeOut;
    vst_opcode_stats_t& getOpCodeStats(bool incoming, int32_t opCode) {
        auto& map = incoming ? opCodeIn : opCodeOut;
        return map[opCode];
    }
    std::vector<String> inputNames;
    std::vector<String> outputNames;
    vstplugin(handles_t* _handle, int32_t globalId, String _sDir, String sName, int32_t _moduleId)
        : effectbase(std::move(sName), PLUGIN_TYPE_VST, globalId),
          handle(_handle),
          internalModuleId(_moduleId),
          sDir(std::move(_sDir)) {
    }
    ~vstplugin() override;
    void resume() override;
    void sleep() override;

protected:
    void onEnable() override;
    void onDisable() override;

public:
    int getModuleType() override { return internalModuleId >= 0 ? PLUGIN_TYPE_INTERNAL_EFFECT : PLUGIN_TYPE_VST; };

    const char* getDir() const {
        return sDir.c_str();
    }
    bool updateWindow();
    String getInfo(std::vector<String>& list) override;
    int64_t dispatch(
            int32_t opcode,
            int32_t index = 0,
            int64_t value = 0,
            void* ptr     = nullptr,
            float opt     = 0);
    bool getNameString(char* szBuf);
    void printNames();
    bool onClose();
    void onWindowDestroy();
    bool onShow(vst_window* window);
    bool updateWindowSize();
    bool onResize(vst_window* window, ivec2 size);
    ivec2 constrainSize(vst_window* window, ivec2& size);
    bool show() override;
    bool close() override;
    void unload(vsthost* host, int flags) override;
    void load(vsthost* host) override;
    void postLoad();
    vst_param_category* getCategory(int idx);
    void recvParamDisplayValueUpdate(int32_t internalIdx);
    void recvProgramNameUpdate();
    void recvProgramListUpdate();

    // automatable_t interface
    String getAutomatableName() override;
    float getParamValue(int32_t idx) override;
    String getParamValueDisplay(int32_t idx) override;
    void setParamValue(int32_t idx, float val, int flags) override;
    automationlane_snapshot_t toRef() const override;
    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;

    bool setCurrentProgram(uint32_t idx) override;
    bool getCurrentProgram(uint32_t& idx) override;
    bool getNumberOfPrograms(uint32_t& numPrograms) override;
    bool getCurrentProgramName(String& out) override;

    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_snapshot_t& pluginSnapshot) override;
    guiplugin* makeGui() override;
    guiplugin* getGui() override;
    void process(AudioBlock* in, AudioBlock* out, double tick, int32_t samplePos, int32_t numSamples, playback_state state) override;
    int32_t getPluginLatency() override;
    int32_t getFlagsVST();
    VstTimeInfo* getLocalTimeInfoPtr();
    uint32_t getLocalCurrentUniqueId();
};
