#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include "automation.h"
#include "str_util.h"
#include "seq_time.h"
#include "assert_dbg.h"
#include "exceptions.h"

#define FLG_PAR_UPDATE_INIT 1
#define FLG_PAR_UPDATE_USER 2
#define FLG_PAR_UPDATE_UNDO 4
#define FLG_PAR_UPDATE_AUTOMATED 8
#define FLG_PAR_UPDATE_NOSTORE 16
#define FLG_PAR_UPDATE_FINISH 32
#define PARAM_ENABLE 0
#define PARAM_GAIN 1
#define PARAM_PAN 2
#define AUTOMATABLE_MIXER 0
#define AUTOMATABLE_ARP 1
#define AUTOMATABLE_EFFECT 2

struct automationlane_snapshot_t {
    int32_t type         = -1;
    int32_t refId        = -1;
    int32_t paramIdx     = -1;
    int32_t height       = 4;
    int32_t subtrackType = 0;
};
//TODO: toRef should return this class:
struct automatble_ref_t {
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
    int32_t quantizationSteps = 0;
    bool active               = true;
    std::vector<automation_point_t> points;
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
};

struct automated_param_t {
    int32_t paramIdx = -1;
    automation_t src;
    explicit automated_param_t(int32_t _paramIdx) : paramIdx(_paramIdx) {}
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
struct automatable_param_t {
    int32_t idx        = -1;
    float defaultValue = 0.0f;
    float value        = 0.0f;
    bool inUse         = false;
    int32_t flags      = 0;

    param_step_fi_u min{ 0.0f };
    param_step_fi_u max{ 1.0f };
    param_step_fi_u stepSmall{ 0.0f };
    param_step_fi_u step{ 0.0f };
    param_step_fi_u stepLarge{ 0.0f };

    String name;
    String unit;
    String shortLabel;

    //if kVstParameterSupportsDisplayIndex
    int16_t displayIndex = 0;///< index where this parameter should be displayed (starting with 0)

    //if kVstParameterSupportsDisplayCategory
    int16_t category    = 0;///< 0: no category, else group index + 1
    int32_t internalIdx = -1;
    String paramDisplayValStr;
    uint8_t paramDisplayValState = 0; 
    uint8_t paramValueState = 0; 
};

struct automatable_t {
private:
    std::unordered_map<int32_t, automatable_param_t> mapParams;
    std::vector<automated_param_t> automatedParams;//TODO: make this a map
public:
    virtual ~automatable_t() = default;
    automatable_param_t* registerParam(int32_t identifier) {
        if (mapParams.find(identifier) != mapParams.end()) {
            throw applogicexception("Param with identical identifier already registered");
        }
        automatable_param_t newParam;
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
    virtual automationlane_snapshot_t toRef() const               = 0;
    virtual track_t* getTrack()                                   = 0;

    virtual void flipParamValue(int32_t idx) {
        setParamValue(idx, 1.0f - getParamValue(idx), FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FINISH);
    }
    virtual void resetParamValue(int32_t paramIdx, int flags) {
        auto it = mapParams.find(paramIdx);
        dbgassert(it != mapParams.end());
        setParamValue(paramIdx, it->second.defaultValue, flags);
    };
    // TODO: don't create a new automation here. Quantization must be property of the parameter
    int32_t getQuantizationSteps(int32_t idx) {
        automation_t* at = getOrCreateAutomation(idx);
        dbgassert(at);
        return at->quantizationSteps;
    }
    // TODO: don't create a new automation here. Quantization must be property of the parameter
    float quantizeVal(int32_t idx, float f) {
        automation_t* at = getOrCreateAutomation(idx);
        dbgassert(at);
        f = quantizeFloat(f, at->quantizationSteps);
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
    virtual void updateAutomatedParameters(tick_t pos) {
        for (automated_param_t& param : automatedParams) {
            if (param.src.isActive()) {
                float val = param.src.getValueAt(pos);
                setParamValue(param.paramIdx, val, FLG_PAR_UPDATE_AUTOMATED);
            }
        }
    }
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
    /**
     * returns: reference
     */
    automatable_param_t* getParamUnchecked(int32_t paramIdx) {
        dbgassert(mapParams.count(paramIdx));
        return &mapParams[paramIdx];
    }
    const automation_t* getRegisteredConstAutomation(int32_t paramIdx) const {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automatedParams.cbegin(), automatedParams.cend(), [paramIdx](const automated_param_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automatedParams.end()) {
            const automated_param_t* ap = &(*it);
            if (ap->src.isAutomated())
                return &(*it).src;
        }
        return nullptr;
    }
    const automation_t* getActiveAutomation(int32_t paramIdx) const {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automatedParams.cbegin(), automatedParams.cend(), [paramIdx](const automated_param_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automatedParams.end()) {
            const automated_param_t* ap = &(*it);
            if (ap->src.isAutomated() && ap->src.isActive())
                return &(*it).src;
        }
        return nullptr;
    }
    automation_t* getRegisteredAutomation(int32_t paramIdx) {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automatedParams.begin(), automatedParams.end(), [paramIdx](automated_param_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automatedParams.end()) {
            automated_param_t* ap = &(*it);
            if (ap->src.isAutomated())
                return &(*it).src;
        }
        return nullptr;
    }
    automation_t* getOrCreateAutomation(int32_t paramIdx) {
        dbgassert(mapParams.count(paramIdx));
        auto it = std::find_if(automatedParams.begin(), automatedParams.end(), [paramIdx](automated_param_t& ap) {
            return ap.paramIdx == paramIdx;
        });
        if (it != automatedParams.end()) {
            return &(*it).src;
        }
        automatedParams.emplace_back(paramIdx);
        return &automatedParams.back().src;
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

class vsthost;
namespace DAW {
    struct automation_ref_t {
        int type  = -1;
        float val = 0.0f;
        automationlane_snapshot_t snapshot{};
    };
    inline automation_ref_t AutomationRef(const automatable_t* automatable, int32_t paramIdx) {
        automationlane_snapshot_t subtrackSnapshot;
        subtrackSnapshot          = automatable->toRef();
        subtrackSnapshot.paramIdx = paramIdx;
        return automation_ref_t{ 1, 0.0f, subtrackSnapshot };
    }
    inline automation_ref_t AutomationConstant(float val) {
        return automation_ref_t{ 0, val, {} };
    }
    bool resolveAutomationAtTime(const vsthost* host, const automation_ref_t& ref, tick_t atTime, float* fOut);
    bool resolveAutomatableRef(const vsthost* host, automationlane_snapshot_t& ref, automatable_t** out);
}// namespace DAW
