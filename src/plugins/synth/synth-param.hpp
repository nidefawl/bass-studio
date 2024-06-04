#pragma once
#include "types.h"
#include "str_util.h"
#include "host/automation/automation.h"

namespace PluginSynth {

struct SynthParam {
    enum class ParamType {
        FLOAT,
        INT,
        ENUM,
    };
    virtual ~SynthParam() = default;
};

struct SynthParamBase : public SynthParam {
    double valDouble    = 0.0;
    double valModulated = 0.0;
    double valInitial   = 0.0;
    ParamType type;
    int32_t enumParam;
    String name;
    String shortName;
    String hierarchicalName; // shortest name: for use in hierarchical UIs
    String format;
    String unit;
    SynthParamBase(ParamType _type, int32_t _enumParam) : type(_type), enumParam(_enumParam) {
    }
    ParamType getType() {
        return this->type;
    }

    ~SynthParamBase() override= default;
    double getAsDouble() const noexcept {
        return valDouble;
    }
    double getAsDoubleModulated(double dModAdd = 0.0) const noexcept {
        return math::clamp(valModulated + dModAdd, 0.0, 1.0);
    }
    void resetToInitial() noexcept {
        valDouble = valModulated = valInitial;
    }
    virtual void set(double f, double fModulated) noexcept = 0;
    void setAll(double f) noexcept {
        set(f, f);
    }
    virtual void setModulated(double f) noexcept {
        set(getAsDouble(), f);
    };
    virtual String getValueDisplay(double value) const = 0;
    virtual param_converted_t convertValueDisplay(const param_unit_t& displayValue) const = 0;
    const String& getName() const {
        return this->name;
    }
    const String& getShortName() const {
        return this->shortName;
    }
    const String& getHierarchicalName() const {
        return this->hierarchicalName;
    }
    const String& getFormat() const {
        return this->format;
    }
    const String& getUnit() const {
        return this->unit;
    }
};

struct SynthParam_Float final : public SynthParamBase {
    explicit SynthParam_Float(int32_t _enumParam) : SynthParamBase(ParamType::FLOAT, _enumParam) {
    }
    double fmin         = 0.0;
    double fmax         = 1.0;
    SynthParam_Float* setRange(double _fmin, double _fmax) {
        fmin = _fmin;
        fmax = _fmax;
        return this;
    }
    double Value() const noexcept {
        return math::clamp(valModulated * (fmax - fmin) + fmin, fmin, fmax);
    }
    double ValueModulated(double voiceModulation) const noexcept {
        return math::clamp((valModulated + voiceModulation) * (fmax - fmin) + fmin, fmin, fmax);
    }
    void setInitialValue(double f) {
        valInitial = math::clamp((math::clamp(f, fmin, fmax) - fmin) / (fmax - fmin), 0.0, 1.0);
        resetToInitial();
    }
    double GetMin() {
        return fmin;
    }
    double GetMax() {
        return fmax;
    }
    void set(double f, double fModulated) noexcept override {
        valDouble = f;
        valModulated = fModulated;
    }
    String getValueDisplay(double value) const noexcept override {
        return StringFormat(StringAsCStr(format), math::clamp((value) * (fmax - fmin) + fmin, fmin, fmax));
    }
    param_converted_t convertValueDisplay(const param_unit_t& displayValue) const override {
        auto val  = atof(StringAsCStr(displayValue.value));
        auto fVal = math::max(0.0, math::min(1.0, (val - fmin) / (fmax - fmin)));
        return { static_cast<float>(fVal), true };
    }
};

struct SynthParam_Int : public SynthParamBase {
    explicit SynthParam_Int(int32_t _enumParam) : SynthParamBase(ParamType::INT, _enumParam) {
    }
    SynthParam_Int(ParamType _paramType, int32_t _enumParam) : SynthParamBase(_paramType, _enumParam) {
    }
    int32_t iMin        = 0;
    int32_t iMax        = 1;
    SynthParam_Int* setRange(int32_t _iMin, int32_t _iMax) {
        iMin = _iMin;
        iMax = _iMax;
        return this;
    }
    int32_t getInt32(double value) const noexcept {
        return math::clamp(math::rounddS32(valModulated * (iMax - iMin) + iMin), iMin, iMax);
    }
    int32_t Value() const noexcept {
        return getInt32(valModulated);
    }
    double ValueModulated(double voiceModulation) const noexcept {
        const double dMin       = iMin;
        const double dMax       = iMax;
        const double dModulated = (dMax - dMin) * (valModulated + voiceModulation) + dMin;
        return math::clamp<double>(dModulated, dMin, dMax);
    }
    void set(double f, double fModulated) noexcept override {
        valDouble  = f;
        valModulated = fModulated;
    }
    void setInitialValue(int32_t i) noexcept {
        valInitial = math::clamp((math::clamp(i, iMin, iMax) - iMin) / static_cast<double>(iMax - iMin), 0.0, 1.0);
        resetToInitial();
    }
    String getValueDisplay(double value) const noexcept override {
        return StringFormat(StringAsCStr(format), math::clamp(math::rounddS32(value * (iMax - iMin) + iMin), iMin, iMax));
    }
    param_converted_t convertValueDisplay(const param_unit_t& displayValue) const override {
        auto val  = atof(StringAsCStr(displayValue.value));
        auto iVal = math::clamp(math::rounddS32(val), iMin, iMax);
        auto dVal = math::clamp((iVal - iMin) / static_cast<double>(iMax - iMin), 0.0, 1.0);
        return { static_cast<float>(dVal), true };
    }
};
struct SynthParam_Enum final : public SynthParam_Int {
    explicit SynthParam_Enum(int32_t _enumParam) : SynthParam_Int(ParamType::ENUM, _enumParam) {
    }
    std::vector<String> strings;
    template<typename StrCtrIt>
    SynthParam_Enum* setStrings(const StrCtrIt& begin, const StrCtrIt& end) {
        strings    = std::vector<String>(begin, end);
        this->iMax = CtrSize(strings) - 1;
        return this;
    }
    String getValueDisplay(double value) const noexcept override {
        int val = math::clamp(math::rounddS32(value * (iMax - iMin) + iMin), iMin, iMax);
        if (val >= 0 && val < CtrSize(strings)) {
            return strings[val];
        }
        return StringFormat("%d", val);
    }
    template<typename T>
    T getEnumValue() const noexcept {
        return static_cast<T>(Value());
    }
};

} // namespace PluginSynth
