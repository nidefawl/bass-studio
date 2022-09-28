#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include "automation.h"
#include "str_util.h"
#include "seq_time.h"
#include "assert_dbg.h"
#include "exceptions.h"


enum param_update_flags : int32_t {
    FLG_PAR_UPDATE_INIT = 1,
    FLG_PAR_UPDATE_USER = 2,
    FLG_PAR_UPDATE_UNDO = 4,
    FLG_PAR_UPDATE_AUTOMATED = 8,
    FLG_PAR_UPDATE_NOSTORE = 16,
    FLG_PAR_UPDATE_FINISH = 32,
    FLG_PAR_UPDATE_FROM_CLIENT = 64,
};

#define PARAM_ENABLE 0
#define PARAM_GAIN 1
#define PARAM_PAN 2

enum automatable_type_t {
    AUTOMATABLE_NONE = -1,
    AUTOMATABLE_MIXER = 0,
    AUTOMATABLE_ARP,
    AUTOMATABLE_EFFECT,
    AUTOMATABLE_MODULATION_SRC,
};

struct automatable_param_ref_t {
    automatable_type_t type = AUTOMATABLE_NONE;
    int32_t refId           = -1;
    int32_t paramIdx        = -1;
    int32_t height          = 4;
    int32_t subtrackType    = 0;
};
namespace DAW {
    struct automation_channel_ref {
        int32_t idx = -1;
        automatable_param_ref_t ref{};
    };
}
//TODO: toRef should return this class:
struct automatable_ref_t {
    // audio_stage_ref_t trackRef
    // int32_t projectGlobalIdEffectRef
};

class track_t;
struct automation_point_t {
    tick_t time;
    float val;
};
inline float quantizeFloat(float f, int32_t steps) {
    if (!steps)
        return f;
    auto val = static_cast<int32_t>((float) steps * f);
    return (float) val / (float) steps;
}
struct automation_t {
    bool active               = true;
    std::vector<automation_point_t> points;
    int32_t quantizationSteps = 0;
    automation_t()          = default;
    virtual ~automation_t() = default;
    virtual bool isActive() const {
        return active && !points.empty();
    }
    virtual bool isAutomated() const {
        return !points.empty();
    }
    float getValueAt(tick_t tick) const;
    float getValueAtExact(double dTick) const;
    void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, float* out) const;
    void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const;
    void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data);
    std::pair<float, float> getMinMax();
};

struct automation_view_t : public automation_t {
    int32_t targetParam = -1;
};

struct automation_clipboard_t {
    tick_t start;
    tick_t len;
    std::vector<automation_point_t> dataPoints;
    automatable_param_ref_t paramRef; 
};

struct automated_param_t {
    int32_t paramIdx = -1;
    automation_t src{};
    automated_param_t() = default;
    automated_param_t(int32_t _paramIdx, int32_t quantizationSteps) : paramIdx(_paramIdx) {
        src.quantizationSteps = quantizationSteps;
    }
    virtual ~automated_param_t() = default;
    virtual bool isActive() const {
        return src.active && !src.points.empty();
    }
    virtual bool isAutomated() const {
        return !src.points.empty();
    }
    virtual float getValueAt(tick_t tick) const {
        return src.getValueAt(tick);
    }
    virtual float getValueAtExact(double dTick) const {
        return src.getValueAtExact(dTick);
    }
    virtual void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, float* out) const {
        //TODO: write optimal version!
        for (samplecount_t i = 0; i < numSamples; i++) {
            double dTick = dTickBegin + (dTickEnd - dTickBegin) * i / (numSamples - 1);
            *out++ = getValueAtExact(dTick);
        }
        // src.sampleAutomation(dTickBegin, dTickEnd, numSamples, out);
    }
};
struct automated_param_connection_t {
    int32_t paramIdx = -1;
    const automated_param_t* param = nullptr;
};

union param_step_fi_u {
    float valFloat;
    int32_t valInt;
};
struct param_unit_t {
    String value;
    String unit;
};
struct param_converted_t {
    float floatVal;
    bool success;
};
struct param_modulation_range_t {
    int32_t sourceId;
    int32_t paramIdx;
    double range;
    bool isBiPolar;
};
enum plugin_param_sync_state : uint8_t {
    PARAM_FLAG_DIRTY = 1,
    PARAM_FLAG_SET = 2
};
struct automatable_param_properties_t {
    int32_t quantizationSteps = 0;
    int32_t displayIndex = 0;
    int32_t flags      = 0;
    String name;
    String unit;
    String shortLabel;
    String paramDisplayValStr;
    uint8_t paramDisplayValState = 0; 
    uint8_t paramValueState = 0; 
    bool inUse         = false;
};
struct automatable_param_t : public automatable_param_properties_t {
    int32_t idx        = -1;
    int32_t internalIdx = -1;
    float defaultValue = 0.0f;
    float value        = 0.0f;
};

struct automatable_t {
private:
    int32_t nextRegisterId = 0;
    std::unordered_map<int32_t, automatable_param_t> mapParams;
    std::vector<automated_param_t> automatedParams;//TODO: make this a map

public:
    std::vector<DAW::automation_channel_ref> inputChannelsAutomation;
    virtual ~automatable_t() = default;
    automatable_param_t* registerParam(int32_t identifier) {
        if (mapParams.find(identifier) != mapParams.end()) {
            throw applogicexception("Param with identical identifier already registered");
        }
        automatable_param_t newParam;
        newParam.displayIndex = nextRegisterId++;
        newParam.idx = identifier;
        mapParams[identifier] = std::move(newParam);
        return &mapParams[identifier];
    }
    template<typename Functor>
    void visitParams(Functor f) {
        std::for_each(mapParams.begin(), mapParams.end(), f);
    }
    template<typename Functor>
    void visitAutomatedParams(Functor f) {
        std::for_each(automatedParams.begin(), automatedParams.end(), f);
    }
    void getSortedParams(std::vector<automatable_param_t*>& _out) {
        _out.reserve(mapParams.size());
        std::for_each(mapParams.begin(), mapParams.end(), [&_out](auto& mapEntry) {
            _out.push_back(&mapEntry.second);
        });
        std::sort(_out.begin(), _out.end(), [](const automatable_param_t* a, const automatable_param_t* b) {
            return a->idx < b->idx;
        });
    }
    void getSortedParamsSeperate(std::vector<automatable_param_t*>& _outAutomated, std::vector<automatable_param_t*>& _outRest) {
        _outAutomated.reserve(automatedParams.size());
        _outRest.reserve(mapParams.size());
        std::for_each(mapParams.begin(), mapParams.end(), [&](auto& mapEntry) {
            const auto paramIdx = mapEntry.first;
            auto it = std::find_if(automatedParams.cbegin(), automatedParams.cend(), [paramIdx](const auto& ap) {
                return ap.paramIdx == paramIdx && ap.src.isAutomated();
            });
            if (it == automatedParams.cend()) {
                _outRest.push_back(&mapEntry.second);
            } else {
                _outAutomated.push_back(&mapEntry.second);
            }
        });
        std::sort(_outAutomated.begin(), _outAutomated.end(), [](const auto* a, const auto* b) {
            return a->idx < b->idx;
        });
        std::sort(_outRest.begin(), _outRest.end(), [](const auto* a, const auto* b) {
            return a->idx < b->idx;
        });
    }

    virtual String getAutomatableName()      = 0;
    virtual float getParamValue(int32_t idx) = 0;
    virtual param_unit_t getParamValueDisplay(int32_t idx);
    virtual param_unit_t convertParamValueToDisplay(int32_t idx, float value);
    virtual param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue);
    /**
     * setParamValue
     * @param idx
     * @param val
     * @param flags valid flags are
     * #define FLG_PAR_UPDATE_INIT 1
     * #define FLG_PAR_UPDATE_USER 2
     * #define FLG_PAR_UPDATE_UNDO 4
     * #define FLG_PAR_UPDATE_AUTOMATED 8
     *
     */
    virtual void setParamValue(int32_t idx, float val, int flags) = 0;
    virtual automatable_param_ref_t toRef() const               = 0;
    virtual track_t* getTrack()                                   = 0;

    virtual void setParamEdit(int32_t idx, float val, int flags);

    virtual void flipParamValue(int32_t idx) {
        setParamValue(idx, 1.0f - getParamValue(idx), FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
    }
    virtual void resetParamValue(int32_t paramIdx, int flags) {
        auto it = mapParams.find(paramIdx);
        dbgassert(it != mapParams.end());
        setParamValue(paramIdx, it->second.defaultValue, flags);
    };
    // TODO: don't create a new automation here. Quantization must be property of the parameter
    int32_t getQuantizationSteps(int32_t paramIdx) {
        auto it = mapParams.find(paramIdx);
        dbgassert(it != mapParams.end());
        return it->second.quantizationSteps;
    }
    // TODO: don't create a new automation here. Quantization must be property of the parameter
    float quantizeVal(int32_t paramIdx, float f) {
        auto it = mapParams.find(paramIdx);
        dbgassert(it != mapParams.end());
        f = quantizeFloat(f, it->second.quantizationSteps);
        return f;
    }

    size_t getNumParameters() const {
        return mapParams.size();
    }
    String getParamName(int32_t paramIdx) {
        auto it = mapParams.find(paramIdx);
        dbgassert(it != mapParams.end());
        return it->second.name;
    }
    void getAutomated(std::vector<int32_t>& targets) {
        for (const automated_param_t& t : automatedParams) {
            if (t.src.isAutomated())
                targets.push_back(t.paramIdx);
        }
    }
    virtual void updateAutomatedParameters(tick_t processingPos, const std::vector<automated_param_connection_t>& modulations);
    void deactivateAutomation(int32_t paramIdx) {
        for (automated_param_t& param : automatedParams) {
            if (paramIdx == param.paramIdx) {
                param.src.active = false;
                return;
            }
        }
    }
    /**
     * returns: null or temporary reference, do not keep around
     */
    automatable_param_t* getEffectParam(int32_t internalIdx) {
        auto it = std::find_if(mapParams.begin(), mapParams.end(), [internalIdx](const auto& mapEntry) {
            return mapEntry.second.internalIdx == internalIdx;
        });
        if (it != mapParams.end()) {
            return &it->second;
        }
        return nullptr;
    }
    /**
     * returns: null or temporary reference, do not keep around
     */
    automatable_param_t* getParam(int32_t paramIdx) {
        auto it = mapParams.find(paramIdx);
        // dbgassert(it != mapParams.end());
        if (it != mapParams.end()) {
            return &it->second;
        }
        return nullptr;
    }
    const automatable_param_t* getParam(int32_t paramIdx) const {
        auto it = mapParams.find(paramIdx);
        // dbgassert(it != mapParams.end());
        if (it != mapParams.end()) {
            return &it->second;
        }
        return nullptr;
    }
    /**
     * returns: reference
     */
    automatable_param_t* getParamUnchecked(int32_t paramIdx) {
        dbgassert(mapParams.count(paramIdx));
        return &mapParams[paramIdx];
    }
    const automatable_param_t* getParamUnchecked(int32_t paramIdx) const {
        dbgassert(mapParams.count(paramIdx));
        return &mapParams.find(paramIdx)->second;
    }
    const automated_param_t* getRegisteredConstAutomation(int32_t paramIdx) const {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automatedParams.cbegin(), automatedParams.cend(), [paramIdx](const automated_param_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automatedParams.end()) {
            const automated_param_t* ap = &(*it);
            if (ap->isAutomated())
                return &(*it);
        }
        return nullptr;
    }
    const automated_param_t* getActiveAutomation(int32_t paramIdx) const {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automatedParams.cbegin(), automatedParams.cend(), [paramIdx](const automated_param_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automatedParams.end()) {
            const automated_param_t* ap = &(*it);
            if (ap->isAutomated() && ap->isActive())
                return &(*it);
        }
        return nullptr;
    }
    automated_param_t* getRegisteredAutomation(int32_t paramIdx) {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automatedParams.begin(), automatedParams.end(), [paramIdx](automated_param_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automatedParams.end()) {
            automated_param_t* ap = &(*it);
            if (ap->isAutomated())
                return ap;
        }
        return nullptr;
    }
    automated_param_t* getOrCreateAutomation(int32_t paramIdx) {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automatedParams.begin(), automatedParams.end(), [paramIdx](automated_param_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automatedParams.end()) {
            return &(*it);
        }
        automatedParams.emplace_back(paramIdx, mapParams[paramIdx].quantizationSteps);
        return &automatedParams.back();
    }
    void getAllAutomatedParams(std::vector<automated_param_t>& out) {
        for (automated_param_t& t : automatedParams) {
            if (t.src.isAutomated()) {
                out.push_back(t);
            }
        }
    }
    virtual void postSetParameter(int32_t idx, float preVal, float val, int flags) {
    }
    std::pair<float, float> getParamMinMaxAutomated(int32_t paramIdx) {
        auto it = std::find_if(automatedParams.begin(), automatedParams.end(), [paramIdx](automated_param_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automatedParams.end()) {
            auto& param = *it;
            if (param.src.isActive()) {
                return param.src.getMinMax();
            }
        }
        automatable_param_t* param = getParamUnchecked(paramIdx);
        dbgassert(param);
        return { param->value, param->value };
    }
    void clearAutomations() {
        this->automatedParams.clear();
    }
};


void loadAutomation(const std::vector<automation_view_t>& automatedParams, automatable_t* at);
void storeAutomation(std::vector<automation_view_t>& automatedParams, automatable_t* at);
int32_t indexOfTick(const std::vector<automation_point_t>& dataPoints, tick_t tick);
int32_t addPointAt(std::vector<automation_point_t>& dataPoints, tick_t tick, int32_t quantizationSteps, float fInitialVal);
void simplifyData(std::vector<automation_point_t>& data);
void toggleDeviceEnableState(automatable_t* effect, int flags);

namespace DAW::Host {
    class PluginManager;
}
namespace DAW {
    enum class automation_routing_type {
        ROUTING_NONE,
        ROUTING_PARAM,
        ROUTING_MODULATION,
    };

    struct automation_routing_t {
        automation_routing_type type  = automation_routing_type::ROUTING_NONE;
        float val = 0.0f;
        automatable_param_ref_t refLane{};
    };
    inline automation_routing_t AutomationRef(const automatable_t* automatable, int32_t paramIdx) {
        automatable_param_ref_t subtrackSnapshot;
        subtrackSnapshot          = automatable->toRef();
        subtrackSnapshot.paramIdx = paramIdx;
        return automation_routing_t{ automation_routing_type::ROUTING_PARAM, 0.0f, subtrackSnapshot };
    }
    inline automation_routing_t AutomationNone(float val) {
        return automation_routing_t{ automation_routing_type::ROUTING_NONE, val, {} };
    }
    // bool resolveAutomationAtTime(const DAW::Host::PluginManager* host, const automation_routing_t& ref, tick_t atTime, float* fOut);
    const automated_param_t* ResolveModulationChannel(const Host::PluginManager* const host, const DAW::automation_channel_ref& ref);
    void ResolveModulationInputRoutings(const Host::PluginManager* const host, const std::vector<DAW::automation_channel_ref>& inputs, std::vector<automated_param_connection_t>& modulations);
    automatable_t* resolveAutomatableRefDevice(const Host::PluginManager* const host, const automatable_param_ref_t& ref);
    const   automated_param_t* GetAutomationSrc(const Host::PluginManager* const host, const automation_routing_t routing);
    automation_routing_t GetAutomationRouting(const automatable_t* dev, int32_t paramIdx);

    void ConnectModulationInputChannel(automatable_t* dev, int32_t paramIdx, DAW::automation_channel_ref ref);
    void DisonnectModulationInputChannel(automatable_t* dev, int32_t paramIdx);
    inline bool IsParamModulated(automatable_t* dev, int32_t paramIdx) {
        for (auto& mod : dev->inputChannelsAutomation) {
            if (mod.idx == paramIdx)
                return true;
        }
        return false;
    }
}// namespace DAW
