#pragma once

#include <utility>

#include "math/vec.h"
#include "host/plugin/modules.h"
#include "str_util.h"
#include "seq_time.h"

#include "host/automation/automation.h"
#include "logging.h"
#include "platform.h"
#include "host/meter/meter.h"
#include "snapshot/snapshot.h"
#include "host/plugin/base/base-plugin.h"

struct AudioBlock;
struct handles_t;
class track_t;
class guibase;
struct track_impl_t;
class host_plugin_window;
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

enum vst_workarounds : uint64_t {
    VST2_R4_BUG_STEREO_PLUGIN_REPORTS_MONO = 1,
    VST2_BUG_NEED_SHOW_WINDOW_TO_LOAD_PRESET = 2
};

class vstplugin : public effectbase {
public:
    handles_t* const handle;
    /** -1 for external, >= 0 for internal plugins */
    const int32_t internalModuleId;
    String sDir;
    bool bWantsEffIdle     = false;
    bool bIsLoadingProgram = false;
    bool bIsPostInit       = false;
    int32_t pluginCategory = 0;
    int32_t vstVersion     = 0;
    int32_t vendorVersion  = 0;
    uint32_t uId           = 0;
    bool isInSuspend   = true;
    uint64_t bugfixFlags = 0;

    std::vector<vst_param_category> paramsCategories;

    //TODO: this is not thread safe
    std::map<int32_t, vst_opcode_stats_t> opCodeIn;
    std::map<int32_t, vst_opcode_stats_t> opCodeOut;
    vst_opcode_stats_t& getOpCodeStats(bool incoming, int32_t opCode) {
        auto& map = incoming ? opCodeIn : opCodeOut;
        return map[opCode];
    }
public:
    vstplugin(handles_t* _handle, int32_t globalId, IHostCallback* hostcallback, String _sDir, String sName, int32_t _moduleId, int32_t _bugfixFlags);
    ~vstplugin() override;
    void onEnable() override;
    void onDisable() override;
    int getModuleType() override { return internalModuleId >= 0 ? PLUGIN_TYPE_INTERNAL_EFFECT : PLUGIN_TYPE_VST; };

    const char* getDir() const {
        return sDir.c_str();
    }
    String getInfo(std::vector<String>& list) override;
    int64_t dispatch(
            int32_t opcode,
            int32_t index = 0,
            int64_t value = 0,
            void* ptr     = nullptr,
            float opt     = 0);
    bool getNameString(char* szBuf);
    void onWindowResize(ivec2 size) override;
    bool onShow(host_plugin_window* _window) override;
    bool onClose() override;
    ivec2 constrainWindowSize(host_plugin_window* window, ivec2 size) override;
    bool hasWindowEditor() override;
    bool showWindow(bool bResetPosition) override;
    void updateFromMainThread() override;
    bool updateWindowSize();
    void unload(DAW::Host::PluginManager* host) override;
    void load(DAW::Host::PluginManager* host) override;
    void configureIOChannels();
    void postLoad();
    vst_param_category* getCategory(int idx);
    void recvParamDisplayValueUpdate(int32_t internalIdx);
    void recvProgramNameUpdate();
    void recvProgramListUpdate();

    // automatable_t interface
    String getAutomatableName() override;
    float getParamValue(int32_t idx) override;
    automatable_param_t* getParam(int32_t idx) override;
    param_unit_t getParamValueDisplay(int32_t idx) override;
    param_unit_t convertParamValueToDisplay(int32_t idx, float value) override;
    param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) override;
    void postSetParameter(int32_t idx, float preVal, float val, int flags) override;
    automatable_param_ref_t toRef() const override;

    bool setCurrentProgram(uint32_t idx) override;
    bool getCurrentProgram(uint32_t& idx) override;
    bool getNumberOfPrograms(uint32_t& numPrograms) override;
    bool getCurrentProgramName(String& out) override;

    void makeSnapshot(plugin_snapshot_t& ps, const tracksnapshot_store_opts_t& opts) override;
    void loadSnapshot(const plugin_snapshot_t& pluginSnapshot) override;
    std::shared_ptr<guiplugin> createGuiPlugin(int32_t uuid) override;
    void process(const DAW::Host::Host* const host, AudioBlock* in, AudioBlock* out, double tick, double samplePos, int32_t numSamples, playback_state state) override;
    void processMidi(midi_data_processing_t& midiEvents) override;
    void sendNotesOff() override;
    samplecount_t getPluginLatency() override;
    int32_t getFlagsVST();
    VstTimeInfo* getLocalTimeInfoPtr();
    uint32_t getLocalCurrentUniqueId();
    void addPropertiesTooltip(Table::tbl& table) override;
    void addPropertiesParameterTooltip(Table::tbl& table, int idx) override;
};
