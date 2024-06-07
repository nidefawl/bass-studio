#pragma once
#include "host/automation/automation.h"
#include "math/seq_math.h"
#include "str_util.h"
#include "types.h"
#include <array>
#include <cstdint>
#include <muParser.h>

namespace PluginSynth {

enum ModulationType {
    Function,
    Constant,
    ModulationSource,
    NumModulationTypes,
};

enum ModulationOperator {
    Multiply,
    Add,
    Subtract,
    Divide,
    MultiplyNegative,
    Absolute,
    Clamp,
    Power,
    NumModulationOperators
};

enum ModulationRange {
    Unipolar,
    Bipolar,
    Triangle,
    NumModulationRanges
};

const std::array<const char*, 8> stringsModOp = {
    "*",
    "+",
    "-",
    "/",
    "*(1-x)",
    "Abs",
    "Clamp",
    "Power"
};

static constexpr size_t MAX_MODULATION_INPUT_PARAMS = 20;
struct MathExprParsed {
    std::array<double, MAX_MODULATION_INPUT_PARAMS> inputs{};
    mu::Parser parser;
    int32_t nanInfCounter = 0;
};

struct MathExpr {
    String str = "x";
    std::shared_ptr<MathExprParsed> parsedExpr;
    /**
        * @brief parse the expression and store the parsed expression. 
        *        Throws an exception if the expression is invalid.
        * @param String strExpression the expression to parse
        * @return MathExpr parsed expression
        */
    static MathExpr parse(const String& strExpression, const std::array<const char*, MAX_MODULATION_INPUT_PARAMS>& varNames) {
        MathExpr expr;
        if (strExpression.length()) {
            auto shrdP    = std::make_shared<MathExprParsed>();
            auto& p       = shrdP->parser;
            auto itInputs = shrdP->inputs.begin();
            for (auto& name : varNames) {
                if (*name == 0) {
                    break;
                }
                auto& var = *itInputs;
                p.DefineVar(name, &var);
                ++itInputs;
                if (itInputs == shrdP->inputs.end()) {
                    break;
                }
            }
            p.SetExpr(strExpression);
            p.Eval();
            expr.str        = strExpression;
            expr.parsedExpr = std::move(shrdP);
        }
        return expr;
    }
};

struct ModulationInput {
    ModulationType type      = ModulationType::ModulationSource;
    int32_t src = -1;
    ModulationOperator op    = ModulationOperator::Multiply;
    double value             = 1.0;
    MathExpr function;
    ModulationRange range = ModulationRange::Unipolar;
};

struct ModulationDestination {
    int32_t parameter = -1;
    double range = 1.0;
};

struct Modulation {
    std::vector<ModulationInput> inputs;
    std::vector<ModulationDestination> destinations;
};

using ModulationSourceData = std::array<double, MAX_MODULATION_INPUT_PARAMS>;
class ModulationController {
public:
static constexpr int32_t MAX_MODULATION_OUTPUT_PARAMS = 64;
    struct ModSrcDesc {
        int32_t srcIdx;
        String name;
    };
    struct ModDestDesc {
        int32_t dstIdx;
        int32_t parameterIdx;
        String name;
    };
protected:
    std::vector<Modulation> modulations;
    std::vector<ModSrcDesc> modSourceDescs;
    std::vector<ModDestDesc> modDestDescs;
    std::array<const char*, MAX_MODULATION_INPUT_PARAMS> varNames;

    /* for visualization */
    std::array<double, 64> modulationValuesMin{};
    std::array<double, 64> modulationValuesMax{};
public:
    const std::array<const char*, MAX_MODULATION_INPUT_PARAMS>& getVarNames() const {
        return varNames;
    }

    bool IsBipolarModulation(const Modulation& modulation) const {
        for (auto& source : modulation.inputs) {
            if (source.range == ModulationRange::Bipolar) return true;
        }
        return false;
    }

    int32_t getModulationIdx(int32_t paramIdx) const {
        for (int32_t i = 0; i < CtrSize(modDestDescs); i++) {
            if (modDestDescs[i].parameterIdx == paramIdx) {
                return i;
            }
        }
        return -1;
    }

    bool isParamModulatable(int32_t param) const {
        return getModulationIdx(param) != -1;
    }

    double getModulationAmountMin(int32_t modIdx) const {
        dbgassert(modIdx >= 0 && modIdx < CtrSize(modulationValuesMin));
        return modulationValuesMin[modIdx];
    }

    double getModulationAmountMax(int32_t modIdx) const {
        dbgassert(modIdx >= 0 && modIdx < CtrSize(modulationValuesMax));
        return modulationValuesMax[modIdx];
    }

    const std::vector<ModDestDesc>& getDestinations() const {
        return modDestDescs;
    }

    const std::vector<ModSrcDesc>& getSources() const {
        return modSourceDescs;
    }

    std::optional<std::vector<param_modulation_range_t>> getParamModulationRanges(int32_t modIdx) {
        dbgassert(modIdx >= 0 && modIdx < MAX_MODULATION_OUTPUT_PARAMS);
        //TODO: result can be cached
        std::optional<std::vector<param_modulation_range_t>> result;
        for (auto& mod : modulations) {
            bool bIsBipolar = IsBipolarModulation(mod);
            for (auto& modDest : mod.destinations) {
                if (modDest.parameter == modIdx) {
                    if (!result) {
                        result = std::vector<param_modulation_range_t>();
                    }
                    auto slot = static_cast<int32_t>(&mod - &modulations.front());
                    result->push_back(
                            param_modulation_range_t{
                                    slot,
                                    modDest.parameter,
                                    static_cast<float>(modDest.range),
                                    bIsBipolar });
                }
            }
        }
        return result;
    }
    Modulation* getModulationIfExists(int32_t index) {
        if (index < 0 || index >= CtrSize(modulations)) {
            return nullptr;
        }
        return &modulations[index];
    }
    Modulation& getOrCreateModulation(int32_t index) {
        while (CtrSize(modulations) <= index) {
            modulations.emplace_back();
        }
        return modulations[index];
    }
    int32_t getModulationCount() const {
        return CtrSize(modulations);
    }
    bool setModulationType(int32_t slotIndex, int32_t srcSlotIndex, int32_t typeIdx) {
        auto& modulation = getOrCreateModulation(slotIndex);
        auto numInputs   = CtrSize(modulation.inputs);
        if (typeIdx < 0) {
            if (srcSlotIndex >= 0 && srcSlotIndex < numInputs) {
                // erase entry
                modulation.inputs.erase(modulation.inputs.begin() + srcSlotIndex);
                return true;
            }
            return false;
        }
        const auto modType    = typeIdx >= ModulationType::ModulationSource ? ModulationType::ModulationSource : static_cast<ModulationType>(typeIdx);
        const auto modSrcType = modType == ModulationType::ModulationSource ? typeIdx - ModulationType::ModulationSource : -1;
        if (srcSlotIndex == numInputs) {
            ModulationInput input = {
                modType,
                modSrcType,
                ModulationOperator::Multiply,
                0.0,
                MathExpr{},
                ModulationRange::Unipolar,
            };
            modulation.inputs.emplace_back(std::move(input));
            return true;
        } else if (srcSlotIndex < numInputs) {
            auto& mod = modulation.inputs[srcSlotIndex];
            mod.type  = modType;
            mod.src   = modSrcType;
            return true;
        }
        return false;
    }
    bool setModulationOperator(int32_t index, int32_t idx, int32_t modOperatorIndex) {
        auto& modulation = getOrCreateModulation(index);
        auto numInputs   = CtrSize(modulation.inputs);
        if (modOperatorIndex >= 0 && modOperatorIndex < ModulationOperator::NumModulationOperators && idx < numInputs) {
            auto& mod = modulation.inputs[idx];
            mod.op    = static_cast<ModulationOperator>(modOperatorIndex);
            return true;
        }
        return false;
    }
    bool setModulationConstant(int32_t index, int32_t idx, double constant) {
        auto& modulation = getOrCreateModulation(index);
        auto numInputs   = CtrSize(modulation.inputs);
        if (idx < numInputs) {
            auto& mod = modulation.inputs[idx];
            mod.value = constant;
            return true;
        }
        return false;
    }
    bool setModulationFunction(int32_t index, int32_t idx, MathExpr&& function) {
        auto& modulation = getOrCreateModulation(index);
        auto numInputs   = CtrSize(modulation.inputs);
        if (idx < numInputs) {
            auto& mod    = modulation.inputs[idx];
            mod.function = std::move(function);
            return true;
        }
        return false;
    }
    bool resetModulationFunction(int32_t index, int32_t idx) {
        auto& modulation = getOrCreateModulation(index);
        auto numInputs   = CtrSize(modulation.inputs);
        if (idx < numInputs) {
            auto& mod = modulation.inputs[idx];
            mod.function.parsedExpr.reset();
            return true;
        }
        return false;
    }
    bool setModulationInputRange(int32_t index, int32_t idx, ModulationRange range) {
        auto& modulation = getOrCreateModulation(index);
        auto numInputs   = CtrSize(modulation.inputs);
        if (idx < numInputs) {
            auto& mod = modulation.inputs[idx];
            mod.range = range;
            return true;
        }
        return false;
    }

    bool setModulationDestination(int32_t index, int32_t destIdx, int32_t paramIdx, double range) {
        auto& modulation     = getOrCreateModulation(index);
        auto numDestinations = CtrSize(modulation.destinations);
        if (paramIdx < 0 && destIdx < numDestinations) {
            // erase entry
            modulation.destinations.erase(modulation.destinations.begin() + destIdx);
            return true;
        } else if (paramIdx >= 0 && paramIdx < MAX_MODULATION_OUTPUT_PARAMS && destIdx == numDestinations) {
            modulation.destinations.push_back({ paramIdx, range });
            return true;
        } else if (paramIdx >= 0 && paramIdx < MAX_MODULATION_OUTPUT_PARAMS && destIdx < numDestinations) {
            modulation.destinations[destIdx] = { paramIdx, range };
            return true;
        }
        return false;
    }
    bool setModulationDestRange(int32_t index, int32_t destIdx, double range) {
        auto& modulation     = getOrCreateModulation(index);
        auto numDestinations = CtrSize(modulation.destinations);
        if (destIdx < numDestinations) {
            modulation.destinations[destIdx].range = range;
            return true;
        }
        return false;
    }

    double EvaluateVoiceModulationMathExpr(const MathExpr& expr, std::array<double, MAX_MODULATION_INPUT_PARAMS>& inputModSources) {
        if (expr.parsedExpr) {
            auto& parsedExpr = *expr.parsedExpr;
            auto& inputs     = parsedExpr.inputs;
            dbgassert(inputs.size() == inputModSources.size());
            memcpy(inputs.data(), inputModSources.data(), sizeof(double) * math::min(inputs.size(), inputModSources.size()));
            double dResult = parsedExpr.parser.Eval();
            if (fp_math::isNanOrInfd(dResult)) {
                dResult = 0.0;
                parsedExpr.nanInfCounter++;
            }
            return dResult;
        }
        return 0.0;
    }

    virtual bool isMathEvalEnabled() const {
        return true;
    }

    virtual bool isShowModulationRanges() const {
        return true;
    }

    virtual void ProcessModulations(ModulationSourceData& sources, std::array<double, 64>& voiceModulations) {
        for (auto& modulation : modulations) {
            double modVal                 = 0.0;
            for (size_t j = 0; j < modulation.inputs.size(); j++) {
                sources.front() = modVal;
                // if (modulation.destinations.empty())
                //     continue;
                auto& input   = modulation.inputs[j];
                double srcVal = 0.0;
                switch (input.type) {
                    case ModulationType::ModulationSource:
                        srcVal = sources[input.src + 1];
                        break;
                    case ModulationType::Constant:
                        srcVal = input.value;
                        break;
                    case ModulationType::Function:
                        if (isMathEvalEnabled()) {
                            srcVal = EvaluateVoiceModulationMathExpr(input.function, sources);
                        }
                        break;
                    default:
                        break;
                }
                if (input.range == ModulationRange::Bipolar && input.type != ModulationType::Function) {
                    srcVal = (srcVal * 2.0) - 1.0;
                }
                if (input.range == ModulationRange::Triangle && input.type != ModulationType::Function) {
                    srcVal = std::fabs((srcVal * 2.0) - 1.0);
                }
                if (j > 0 && input.type != ModulationType::Function) {
                    switch (input.op) {
                        case ModulationOperator::Multiply:
                            srcVal = modVal * srcVal;
                            break;
                        case ModulationOperator::Add:
                            srcVal = modVal + srcVal;
                            break;
                        case ModulationOperator::Divide:
                            if (math::abs(srcVal) < 1e-6) {
                                srcVal = 1e-6;
                            } else {
                                srcVal = modVal / srcVal;
                            }
                            break;
                        case ModulationOperator::Subtract:
                            srcVal = modVal - srcVal;
                            break;
                        case ModulationOperator::MultiplyNegative:
                            srcVal = modVal * (1 - srcVal);
                            break;
                        case ModulationOperator::Absolute:
                            srcVal = abs(modVal * srcVal);
                            break;
                        case ModulationOperator::Power:
                            srcVal = exp(log(srcVal) * modVal);
                            break;
                        case ModulationOperator::Clamp:
                            srcVal = math::clamp(modVal, double(input.range == ModulationRange::Bipolar) * -1.0 * srcVal, srcVal);
                            break;
                        default:
                            srcVal = modVal;
                            break;
                    }
                }
                modVal = srcVal;
            }
            for (auto& dest : modulation.destinations) {
                size_t destIdx = dest.parameter;
                voiceModulations[destIdx] += modVal * dest.range;
            }
        }
    }
};

} // namespace PluginSynth
