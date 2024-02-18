#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>
#include "automation.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "seq_time.h"
#include "assert_dbg.h"
#include "exceptions.h"

enum param_update_flags : int32_t {
    FLG_PAR_UPDATE_INIT = 1,
    FLG_PAR_UPDATE_USER = 2,
    FLG_PAR_UPDATE_UNDO = 4,
    FLG_PAR_UPDATE_AUTOMATED = 8,
    FLG_PAR_UPDATE_MODULATED = 16,
    FLG_PAR_UPDATE_NOSTORE = 32,
    FLG_PAR_UPDATE_FINISH = 64,
    FLG_PAR_UPDATE_FROM_CLIENT = 128,
};

#define PARAM_ENABLE 0
#define PARAM_GAIN 1
#define PARAM_PAN 2

enum automatable_type_t {
    AUTOMATABLE_NONE = -1,
    AUTOMATABLE_MIXER = 0,
    AUTOMATABLE_ARP,
    AUTOMATABLE_EFFECT,
    AUTOMATABLE_MODULATOR_OUTPUT
};
struct automatable_param_ref_t {
    automatable_type_t type = AUTOMATABLE_NONE;
    int32_t refId           = -1;
    int32_t paramIdx        = -1;
};

inline bool operator==(const automatable_param_ref_t& lhs, const automatable_param_ref_t& rhs)
{
    return std::tie(lhs.type, lhs.refId, lhs.paramIdx) ==
           std::tie(rhs.type, rhs.refId, rhs.paramIdx);
}

inline bool operator<(const automatable_param_ref_t& lhs, const automatable_param_ref_t& rhs)
{
    return std::tie(lhs.type, lhs.refId, lhs.paramIdx) <
           std::tie(rhs.type, rhs.refId, rhs.paramIdx);
}

namespace DAW {
    enum ModulationMode : uint8_t {
        REPLACE = 0,
        ADD,
        MUL,
        BYPASS,
    };
    struct modulation_scaling_t {
        float min = 0.0f;
        float max = 1.0f;
        ModulationMode mode = ModulationMode::BYPASS;
        bool bClamp = true;
    };
    struct modulation_channel_ref {
        int32_t paramIdxDst = -1;
        automatable_param_ref_t refSrc{};
        modulation_scaling_t scale{};
    };
    enum ModulationChannel : uint8_t {
        NONE = 0,
        PARAM,
        AUTOMATION,
        MODULATOR,
    };
    struct modulation_channel_proc_t {
        ModulationChannel type = ModulationChannel::NONE;
        int32_t index = -1;
        automatable_param_ref_t refSrc{};
        modulation_scaling_t scale{};
    };
    struct modulation_channel_desc {
        channelnum_t offset = 0;
        String name;
    };
}

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
    void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, const DAW::modulation_scaling_t& scale, float* out) const;
    void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const;
    void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data);
    void deleteTickRange(tick_t tickBegin, tick_t tickEnd);
    void insertTickRange(tick_t tickBegin, tick_t tickEnd, const std::vector<automation_point_t>& data);
    std::pair<float, float> getMinMax();
    std::optional<std::pair<tick_t, tick_t>> getBeginEnd() const;
    void activate() {
        active = true;
    }
    void deactivate() {
        active = false;
    }
    bool isActive() {
        return active;
    }
    void setActive(bool bActive) {
        active = bActive;
    }
};

struct automation_view_t final : public automation_t {
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
    automated_param_t() = default;
    virtual ~automated_param_t() = default;
    virtual bool isActive() const = 0;
    virtual bool isAutomated() const = 0;
    virtual float modulateValue(tick_t tick, float f, const DAW::modulation_scaling_t& scale) const = 0;
    virtual float getValueAt(tick_t tick) const = 0;
    virtual float getValueAtExact(double dTick) const = 0;
    virtual void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, const DAW::modulation_scaling_t& scale, float* inOut) const = 0;
    virtual String getName() const = 0;
    virtual void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const = 0;
    virtual void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) = 0;
    virtual void deleteTickRange(tick_t tickBegin, tick_t tickEnd) = 0;
    virtual void insertTickRange(tick_t tickBegin, tick_t tickEnd, const std::vector<automation_point_t>& data) = 0;
    virtual std::optional<std::pair<tick_t, tick_t>> getBeginEnd() const {
        return std::nullopt;
    }
};

struct automation_lane_t final : public automated_param_t {
    automation_t src{};
    automation_lane_t() = default;
    automation_lane_t(int32_t _paramIdx, int32_t quantizationSteps) {
        paramIdx = _paramIdx;
        src.quantizationSteps = quantizationSteps;
    }
    ~automation_lane_t() override = default;
    bool isActive() const override {
        return src.isActive() && !src.points.empty();
    }
    bool isAutomated() const override {
        return !src.points.empty();
    }
    float modulateValue(tick_t tick, float fIn, const DAW::modulation_scaling_t& scale) const override;
    float getValueAt(tick_t tick) const override {
        return src.getValueAt(tick);
    }
    float getValueAtExact(double dTick) const override {
        return src.getValueAtExact(dTick);
    }
    void sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, const DAW::modulation_scaling_t& scale, float* out) const override {
        src.sampleAutomation(dTickBegin, dTickEnd, numSamples, scale, out);
    }
    void copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const override {
        src.copyRange(tickBegin, tickEnd, data);
    }
    void setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) override {
        src.setRange(tickBegin, tickEnd, data);
    }
    void deleteTickRange(tick_t tickBegin, tick_t tickEnd) override {
        src.deleteTickRange(tickBegin, tickEnd);
    }
    void insertTickRange(tick_t tickBegin, tick_t tickEnd, const std::vector<automation_point_t>& data) override {
        src.insertTickRange(tickBegin, tickEnd, data);
    }
    String getName() const override {
        return "Automation";
    }
    std::optional<std::pair<tick_t, tick_t>> getBeginEnd() const override {
        return src.getBeginEnd();
    }
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
    DAW::modulation_scaling_t parameterScaling{0.0f, 1.0f, DAW::ModulationMode::REPLACE, false};
    DAW::modulation_scaling_t automationScale{0.0f, 1.0f, DAW::ModulationMode::REPLACE, false};
    int32_t quantizationSteps = 0;
    int32_t displayIndex = 0;
    int32_t flags      = 0;
    String name;
    String unit;
    String shortLabel;
    String paramDisplayValStr;
    uint8_t paramNameState = 0; 
    uint8_t paramDisplayValState = 0; 
    uint8_t paramValueState = 0; 
    bool inUse         = false;
    bool isBiPolar     = false;
};

struct automatable_param_t final : public automatable_param_properties_t {
    friend struct automatable_t;
    int32_t idx         = -1;
    int32_t internalIdx = -1;

private:
    float defaultValue   = 0.0f;
    float valueModulated = 0.0f;
    float valueAutomated = 0.0f;
    float value          = 0.0f;
    bool bIsModulated   = false;

public:
    template<typename T>
    void initValue(T _value) {
        _value.val = math::clamp(_value.val, 0.0f, 1.0f);
        valueModulated = _value.val;
        defaultValue = _value.val;
        value = _value.val;
        unit = _value.unit;
        name = shortLabel = _value.name;
    }
    void setAll(float _value) {
        valueAutomated = _value;
        valueModulated = _value;
        value = _value;
    }
    void setValue(float _value) {
        value = _value;
    }
    void setInitial(float _value) {
        defaultValue = _value;
        setAll(_value);
    }
    void setModulated(float _value) {
        valueModulated = math::clamp(_value, 0.0f, 1.0f);
    }
    void setAutomated(float _value) {
        valueAutomated = _value;
    }
    float getValue() const {
        return value;
    }
    float getValueModulated() const {
        return valueModulated;
    }
    float getValueAutomated() const {
        return valueAutomated;
    }
    float getDefault() const {
        return defaultValue;
    }
    bool isModulated() const {
        return bIsModulated;
    }
    void setModulated(bool _isModulated) {
        bIsModulated = _isModulated;
    }
    DAW::modulation_scaling_t& getAutomationScale() {
        return automationScale;
    }
    const DAW::modulation_scaling_t& getAutomationScale() const {
        return automationScale;
    }
    void setAutomationScale(const DAW::modulation_scaling_t& _scale) {
        automationScale = _scale;
    }
    DAW::modulation_scaling_t& getParameterScale() {
        return parameterScaling;
    }
    const DAW::modulation_scaling_t& getParameterScale() const {
        return parameterScaling;
    }
    void setParameterScale(const DAW::modulation_scaling_t& _scale) {
        parameterScaling = _scale;
    }
};


class track_t;
namespace DAW::Host {
    class PluginManager;
}
struct automatable_t {
private:
    int32_t nextRegisterId = 0;
    std::unordered_map<int32_t, automatable_param_t> mapParams;
    std::vector<automation_lane_t> automationLanes;//TODO: make this a map
    std::vector<DAW::modulation_channel_ref> inputChannelsModulation;
    std::unordered_map<int32_t, std::vector<DAW::modulation_channel_ref*>> mapModulations;
protected:
    const std::unordered_map<int32_t, std::vector<DAW::modulation_channel_ref*>>& getModulationsMap() const {
        return mapModulations;
    }
    const std::vector<automation_lane_t>& getAutomationLanes() const {
        return automationLanes;
    }
    void setAutomatableParam(automatable_param_t* param, float value, int flags);
public:
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

    void setModulations(const std::vector<DAW::modulation_channel_ref>& inputChannelsModulation);
    void removeModulation(int32_t paramIdx, int32_t modulationIndex);
    void updateModulationMap();
    std::vector<DAW::modulation_channel_ref>& getModulations() {
        return inputChannelsModulation;
    }
    const std::vector<DAW::modulation_channel_ref>& getModulations() const {
        return inputChannelsModulation;
    }
    const std::vector<DAW::modulation_channel_ref*>* getModulations(int32_t paramIdx) const {
        auto it = mapModulations.find(paramIdx);
        if (it != mapModulations.end()) {
            return &it->second;
        }
        return nullptr;
    }

    bool isParamModulated(int32_t paramIdx) const {
        return getParam(paramIdx)->isModulated();
    }

    bool isParamConnectedTo(int32_t paramIdx, const DAW::modulation_channel_ref& modChannel) const;

    template<typename Functor>
    void visitParams(Functor f) {
        std::for_each(mapParams.begin(), mapParams.end(), f);
    }
    template<typename Functor>
    void visitAutomatedParams(Functor f) {
        std::for_each(automationLanes.begin(), automationLanes.end(), f);
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
        _outAutomated.reserve(automationLanes.size());
        _outRest.reserve(mapParams.size());
        std::for_each(mapParams.begin(), mapParams.end(), [&](auto& mapEntry) {
            const auto paramIdx = mapEntry.first;
            auto it = std::find_if(automationLanes.cbegin(), automationLanes.cend(), [paramIdx](const auto& ap) {
                return ap.paramIdx == paramIdx && ap.src.isAutomated();
            });
            if (it == automationLanes.cend()) {
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
    /**
     * setParamValue
     * @param idx
     * @param val
     * @param flags see param_update_flags
     */
    void setParamValue(int32_t idx, float val, int flags) {
        setAutomatableParam(getParamUnchecked(idx), val, flags);
    }
    virtual String getAutomatableName()      = 0;
    virtual automatable_param_ref_t toRef() const = 0;
    virtual track_t* getTrack() = 0;

    virtual float getParamValue(int32_t idx);
    virtual param_unit_t getParamValueDisplay(int32_t idx);
    virtual param_unit_t convertParamValueToDisplay(int32_t idx, float value);
    virtual param_converted_t convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue);
    virtual void setParamEdit(int32_t idx, float val, int flags);

    virtual void flipParamValue(int32_t idx) {
        setParamValue(idx, 1.0f - getParam(idx)->getValue(), FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
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
    virtual String getParamName(int32_t paramIdx) {
        auto it = mapParams.find(paramIdx);
        dbgassert(it != mapParams.end());
        return it->second.name;
    }
    void getAutomated(std::vector<int32_t>& targets) {
        for (const auto& t : automationLanes) {
            if (t.isAutomated())
                targets.push_back(t.paramIdx);
        }
    }
    void sampleAutomation(const DAW::Host::PluginManager *const host, int32_t paramIdx, double dTickBegin, double dTickEnd, playback_state state, samplecount_t numSamples, float* out);
    virtual void updateAutomatedParameters(const DAW::Host::PluginManager *const host, tick_t processingPos, playback_state state);
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
    virtual automatable_param_t* getParam(int32_t paramIdx) {
        auto it = mapParams.find(paramIdx);
        // dbgassert(it != mapParams.end());
        if (it != mapParams.end()) {
            return &it->second;
        }
        return nullptr;
    }
    virtual const automatable_param_t* getParam(int32_t paramIdx) const {
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
    const automation_lane_t* getRegisteredConstAutomation(int32_t paramIdx) const {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automationLanes.cbegin(), automationLanes.cend(), [paramIdx](const automation_lane_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automationLanes.end()) {
            const automation_lane_t* ap = &(*it);
            if (ap->isAutomated())
                return &(*it);
        }
        return nullptr;
    }
    const automation_lane_t* getActiveAutomation(int32_t paramIdx) const {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automationLanes.cbegin(), automationLanes.cend(), [paramIdx](const automation_lane_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automationLanes.end()) {
            const automation_lane_t* ap = &(*it);
            if (ap->isAutomated() && ap->isActive())
                return &(*it);
        }
        return nullptr;
    }
    automation_lane_t* getRegisteredAutomation(int32_t paramIdx) {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automationLanes.begin(), automationLanes.end(), [paramIdx](automation_lane_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automationLanes.end()) {
            automation_lane_t* ap = &(*it);
            if (ap->isAutomated())
                return ap;
        }
        return nullptr;
    }
    automation_lane_t* getOrCreateAutomation(int32_t paramIdx);
    void getAllAutomatedParams(std::vector<automation_lane_t>& out) {
        for (automation_lane_t& t : automationLanes) {
            if (t.src.isAutomated()) {
                out.push_back(t);
            }
        }
    }
    void getAllAutomatedParamRef(std::vector<automation_lane_t*>& out) {
        for (automation_lane_t& t : automationLanes) {
            out.push_back(&t);
        }
    }
    virtual void postSetParameter(int32_t idx, float preVal, float val, int flags) {
        dbgassert(!fp_math::isNanOrInfd(val));
    }
    std::pair<float, float> getParamMinMaxAutomated(int32_t paramIdx) {
        auto it = std::find_if(automationLanes.begin(), automationLanes.end(), [paramIdx](automation_lane_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automationLanes.end()) {
            auto& param = *it;
            if (param.src.isActive()) {
                return param.src.getMinMax();
            }
        }
        automatable_param_t* param = getParamUnchecked(paramIdx);
        dbgassert(param);
        return { param->getValue(), param->getValue() };
    }
    void clearAutomations() {
        this->automationLanes.clear();
    }
};


void loadAutomation(const std::vector<automation_view_t>& automatedParams, automatable_t* at);
void storeAutomation(std::vector<automation_view_t>& automatedParams, automatable_t* at);
int32_t indexOfTick(const std::vector<automation_point_t>& dataPoints, tick_t tick);
int32_t addPointAt(std::vector<automation_point_t>& dataPoints, tick_t tick, int32_t quantizationSteps, float fInitialVal);
void simplifyData(std::vector<automation_point_t>& data);
void toggleDeviceEnableState(automatable_t* effect, int flags);

namespace DAW {
    enum class automation_routing_type {
        ROUTING_NONE,
        ROUTING_CONSTANT,
        ROUTING_LANE,
        ROUTING_MODULATION,
    };

    struct automation_routing_t {
        automation_routing_type type  = automation_routing_type::ROUTING_NONE;
        automatable_param_ref_t destinationRef{};
    };

    inline automation_routing_t AutomationRef(const automatable_t* automatable, int32_t paramIdx) {
        auto atlRef = automatable->toRef();
        atlRef.paramIdx = paramIdx;
        return automation_routing_t{ automation_routing_type::ROUTING_LANE, atlRef };
    }

    inline automation_routing_t ModulationRef(const automatable_t* automatable, int32_t paramIdx) {
        auto atlRef = automatable->toRef();
        atlRef.paramIdx = paramIdx;
        return automation_routing_t{ automation_routing_type::ROUTING_MODULATION, atlRef };
    }

    inline automation_routing_t AutomationConstant(const automatable_t* automatable, int32_t paramIdx) {
        auto atlRef = automatable->toRef();
        atlRef.paramIdx = paramIdx;
        return automation_routing_t{ automation_routing_type::ROUTING_CONSTANT, atlRef };
    }

    inline automation_routing_t AutomationNone() {
        return automation_routing_t{ automation_routing_type::ROUTING_NONE, {} };
    }

    struct automated_param_connection_t {
        automation_routing_type type = automation_routing_type::ROUTING_NONE;
        automatable_t* atl = nullptr;
        int32_t paramIdx = -1;
    };

    automated_param_connection_t GetParameterModulationFromRouting(const Host::PluginManager* const host, const automation_routing_t routing);
    automation_routing_t GetRoutingFromDestinationParam(const automatable_t* dev, int32_t paramIdx);

    automatable_t* resolveAutomatableRefDevice(const Host::PluginManager* const host, const automatable_param_ref_t& ref);
    const automated_param_t* ResolveModulationChannel(const Host::PluginManager* const host, const modulation_channel_ref& ref);
    void ConnectModulationInputChannel(automatable_t* dev, int32_t paramIdx, modulation_channel_ref ref, const modulation_scaling_t& mode);
    void DisonnectModulationInputChannel(automatable_t* dev, modulation_channel_ref ref);
    void DisonnectModulationForParam(automatable_t* dev, int32_t paramIdx);

    inline bool IsParamModulated(automatable_t* dev, int32_t paramIdx) {
        return dev->isParamModulated(paramIdx);
    }
}// namespace DAW
