#include <algorithm>
#include <vector>
#include "assert_dbg.h"
#include "automation.h"
#include "config.h"
#include "host/daw/mainctrl.h"
#include "logging.h"
#include "math/seq_math.h"
#include "host/plugin/vst/vstplugin.h"
#include "gui/automation/automatable.h"
#include "seq_time.h"
#include "seq_util.h"
#include "str_util.h"
#include "dsp_util.h"
#include "types.h"

int32_t indexOfTick(const std::vector<automation_point_t>& dataPoints, tick_t tick) {
    int32_t idx;
    for (idx = 0; idx < (int) dataPoints.size(); idx++) {
        const automation_point_t& pt = dataPoints[idx];
        if (pt.time > tick) {
            break;
        }
    }
    return idx;
}
int32_t addPointAt(std::vector<automation_point_t>& dataPoints, tick_t tick, int32_t quantizationSteps, float fInitialVal) {
    int32_t idx;
    for (idx = 0; idx < (int) dataPoints.size(); idx++) {
        automation_point_t& pt = dataPoints[idx];
        if (pt.time > tick) {
            break;
        }
    }
    if (!dataPoints.empty()) {
        float v;
        if (idx == (int) dataPoints.size()) {
            v = dataPoints[idx - 1].val;
        } else if (idx == 0) {
            v = dataPoints[0].val;
        } else {
            automation_point_t& pt1 = dataPoints[idx - 1];
            if (quantizationSteps) {
                v = pt1.val;
            } else {
                automation_point_t& pt2 = dataPoints[idx];
                dbgassert(tick >= pt1.time && tick <= pt2.time);
                tick_t tickDist = pt2.time - pt1.time;
                if (tickDist == 0) {
                    v = pt2.val;
                } else {
                    /**
                     * NOTE - precission - converting 2 relative tick_t values to float for LERPing
                     * @see automation_t::getValueAt
                     **/
                    float pr = (tick - pt1.time) / (float) tickDist;
                    v = pt1.val + pr * (pt2.val - pt1.val);
                }
            }
        }
        dataPoints.insert(dataPoints.begin() + idx, { tick, v });
        return idx;
    } else {
        dataPoints.insert(dataPoints.begin(), { tick, fInitialVal });
    }
    return 0;
}

void simplifyData(std::vector<automation_point_t>& data) {
    //TODO: proof correctness and compare performance to STL algo

    {
        /* remove multiple points on same time */
        auto first = data.begin();
        auto last  = data.end();
        if (first != last) {
            for (auto it = first; it != last; ++it) {
                tick_t firstTime = (*it).time;
                *first++         = std::move(*it);
                if (it + 1 != last) {
                    std::vector<automation_point_t>::iterator j = it + 2;
                    for (; j < last; ++j) {
                        if (firstTime != (*j).time) {
                            break;
                        }
                    }
                    it = j - 2;
                }
            }
            if (first != last)
                data.erase(first, last);
        }
    }
    {
        /* remove multiple consecutive points with same value */
        auto first = data.begin();
        auto last  = data.end();
        if (first != last) {
            for (auto i = first; i != last; ++i) {
                float firstVal = (*i).val;
                *first++       = std::move(*i);

                if (i + 1 != last) {
                    std::vector<automation_point_t>::iterator j = i + 2;
                    for (; j < last; ++j) {
                        if (firstVal != (*j).val || firstVal != (*(j - 1)).val) {
                            break;
                        }
                    }
                    i = j - 2;
                }
            }
            if (first != last)
                data.erase(first, last);
        }
    }
}
float automation_t::getValueAt(tick_t tick) const {
    if (!points.empty()) {
        int32_t idx = indexOfTick(points, tick);
        dbgassert(idx <= CtrSize(points));
        if (idx == CtrSize(points))
            return points.back().val;
        if (idx > 0) {
            const automation_point_t& pt1 = points[idx - 1];
            if (quantizationSteps) {
                return pt1.val;
            }
            const automation_point_t& pt2 = points[idx];
            dbgassert(tick >= pt1.time && tick <= pt2.time);
            tick_t tickDist = pt2.time - pt1.time;
            if (!tickDist) {
                return pt2.val;
            }
            /**
             * NOTE - precission - converting 2 relative tick_t values to float for LERPing
             * Precision depends on the distance in ticks between consecutive automation points
             **/
            float pr = (tick - pt1.time) / (float) tickDist;
            return pt1.val + pr * (pt2.val - pt1.val);
        }
        return points.front().val;
    }
    return 0.5f;
}
float automation_t::getValueAtExact(double dTick) const {
    if (!points.empty()) {
        tick_t tick = math::floordS32(dTick);
        int32_t idx = indexOfTick(points, tick);
        dbgassert(idx <= CtrSize(points));
        if (idx == CtrSize(points))
            return points.back().val;
        if (idx > 0) {
            const automation_point_t& pt1 = points[idx - 1];
            if (quantizationSteps) {
                return pt1.val;
            }
            const automation_point_t& pt2 = points[idx];
            dbgassert(tick >= pt1.time && tick <= pt2.time);
            tick_t tickDist = pt2.time - pt1.time;
            if (!tickDist) {
                return pt2.val;
            }
            /**
             * NOTE - precission - converting 2 relative tick_t values to float for LERPing
             * Precision depends on the distance in ticks between consecutive automation points
             **/
            auto pr = (dTick - pt1.time) / double(tickDist);
            return static_cast<float>(pt1.val + pr * (pt2.val - pt1.val));
        }
        return points.front().val;
    }
    return 0.5f;
}
void automation_t::sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, const DAW::modulation_scaling_t& scale, float* inOut) const {
    //TODO: write optimal version!
    for (samplecount_t i = 0; i < numSamples; ++i) {
        const auto dTickOffset     = dTickBegin + i * (dTickEnd - dTickBegin) / double(numSamples);
        const auto valScaled = scale.min + getValueAtExact(dTickOffset) * (scale.max - scale.min);
        switch (scale.mode) {
            case DAW::ModulationMode::ADD:
                *inOut++ += valScaled;
                break;
            case DAW::ModulationMode::MUL:
                *inOut++ *= valScaled;
                break;
            case DAW::ModulationMode::REPLACE:
                *inOut++ = valScaled;
                break;
            default:
                break;
            if (scale.bClamp) {
                *inOut = math::clamp(*inOut, 0.0f, 1.0f);
            }
        }
    }
}

void automation_t::copyRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) const {
    if (!points.empty()) {
        int32_t idxStart = indexOfTick(points, tickBegin);
        int32_t idxEnd   = indexOfTick(points, tickEnd) + 1;
        if (idxStart >= (int32_t) points.size()) {
            idxStart = (int32_t) points.size() - 1;
        }
        if (idxEnd >= (int32_t) points.size()) {
            idxEnd = (int32_t) points.size() - 1;
        }
        automation_point_t ptStart{ 0, getValueAt(tickBegin) };
        automation_point_t ptEnd{ tickEnd - tickBegin, getValueAt(tickEnd) };
        int32_t loopLen = math::max(0, (idxEnd) - (idxStart));
        data.reserve(2 + loopLen);
        data.push_back(std::move(ptStart));
        for (int j = idxStart; j <= idxEnd; j++) {
            auto pt = points[j];
            if (pt.time >= tickBegin && pt.time <= tickEnd) {
                pt.time -= tickBegin;
                data.push_back(std::move(pt));
            }
        }
        data.push_back(std::move(ptEnd));
    }
}

std::pair<float, float> automation_t::getMinMax() {
    float defaultVal            = getValueAt(0);
    std::pair<float, float> res = { defaultVal, defaultVal };
    int32_t idx1                = 0;
    for (; idx1 < CtrSize(points); idx1++) {
        auto& pt   = points[idx1];
        res.first  = math::min(res.first, pt.val);
        res.second = math::max(res.second, pt.val);
    }
    return res;
}

std::optional<std::pair<tick_t, tick_t>> automation_t::getBeginEnd() const {
    auto nPoints = CtrSize(points);
    if (nPoints > 0) {
        return std::make_pair(points.front().time, points.back().time);
    }
    return std::nullopt;
}

void automation_t::setRange(tick_t tickBegin, tick_t tickEnd, std::vector<automation_point_t>& data) {
    std::vector<automation_point_t> pointsTmp;
    pointsTmp.reserve(data.size() + points.size());
    int32_t idx1 = 0;
    for (; idx1 < CtrSize(points); idx1++) {
        auto& pt = points[idx1];
        if (pt.time <= tickBegin) {
            pointsTmp.push_back(pt);
        } else {
            break;
        }
    }
    if (!pointsTmp.empty() && !data.empty() && pointsTmp.back().time != tickBegin) {
        automation_point_t ptStart{ tickBegin, getValueAt(tickBegin) };
        pointsTmp.push_back(std::move(ptStart));
    }
    for (int32_t idx = 0; idx < (int) data.size(); idx++) {
        auto pt = data[idx];
        pt.time += tickBegin;
        pt.val = quantizeFloat(pt.val, quantizationSteps);
        pointsTmp.push_back(std::move(pt));
    }
    if (!pointsTmp.empty() && !data.empty()) {
        automation_point_t ptEnd{ tickEnd, getValueAt(tickEnd) };
        pointsTmp.push_back(std::move(ptEnd));
    }
    for (; idx1 < CtrSize(points); idx1++) {
        auto& pt = points[idx1];
        if (pt.time >= tickEnd) {
            pointsTmp.push_back(pt);
        }
    }
    simplifyData(pointsTmp);
    points = std::move(pointsTmp);
}

void toggleDeviceEnableState(automatable_t* effect, int flags) {
    auto paramEnable = effect->getParam(PARAM_ENABLE);
    auto f = paramEnable->getValue() > 0 ? 0 : 1;
    effect->setParamEdit(PARAM_ENABLE, f, flags);
}

void loadAutomation(const std::vector<automation_view_t>& automatedParams, automatable_t* at) {
    if (!automatedParams.empty()) {
        log_lf(Log::L_DEBUG, "Loading %zu automation lanes for device %s\n", automatedParams.size(), StringAsCStr(at->getAutomatableName()));
    }
    at->clearAutomations();
    for (const automation_view_t& automatedParam : automatedParams) {
        int32_t targetParam = automatedParam.targetParam;
        auto param = at->getParam(targetParam);
        if (param) {
            auto* autom = at->getOrCreateAutomation(param->idx);
            autom->src.points = automatedParam.points;
            autom->src.setActive(automatedParam.active);
        } else {
            log_lf(Log::L_WARN, "Param %d missing for device %s\n", automatedParam.targetParam, StringAsCStr(at->getAutomatableName()));
        }
    }
}
void storeAutomation(std::vector<automation_view_t>& automatedParams, automatable_t* at) {
    std::vector<automation_lane_t> out;
    at->getAllAutomatedParams(out);
    int total = 0;
    for (const auto& automatedParam : out) {
        dbgassert(!automatedParam.src.points.empty());
        automation_view_t atv;
        atv.targetParam = automatedParam.paramIdx;
        atv.points      = automatedParam.src.points;
        atv.active      = automatedParam.src.isActive();
        automatedParams.push_back(atv);
        total++;
    }
    if (total) {
        log_lf(Log::L_TRACE, "Storing %d automation lanes for device %s\n", total, StringAsCStr(at->getAutomatableName()));
    }
}

void automatable_t::setParamEdit(int32_t idx, float val, int flags) {
    auto param = getParam(idx);
    auto* automation = getRegisteredAutomation(idx);
    if (automation) {
        auto automationPref = param->getAutomationScale();
        if (automationPref.mode == DAW::ModulationMode::REPLACE) {
            automation->src.deactivate();
        }
    }
    setAutomatableParam(param, val, flags);
}

void automatable_t::setAutomatableParam(automatable_param_t* param, float val, int flags) {
    dbgassert(param);
    float valPre = param->getValue();
    if (flags & FLG_PAR_UPDATE_MODULATED) {
        param->setModulated(val);
    } else if (flags & FLG_PAR_UPDATE_AUTOMATED) {
        param->setAutomated(val);
        // auto automationPref = param->getAutomationScale();
        // if (automationPref.mode == DAW::ModulationMode::REPLACE) {
        //     param->setValue(val);
        // }
    } else {
        param->setValue(val);
    }
    if ((flags & (FLG_PAR_UPDATE_USER | FLG_PAR_UPDATE_FROM_CLIENT)) != 0)
    {
        param->inUse = true;
    }
    if ((flags & FLG_PAR_UPDATE_INIT) && !(flags & FLG_PAR_UPDATE_NOSTORE))
    {
        param->inUse = true;
    }
    postSetParameter(param->idx, valPre, val, flags);
}

float automatable_t::getParamValue(int32_t idx) {
    automatable_param_t* param = getParamUnchecked(idx);
    dbgassert(param);
    if (param->isModulated()) {
        return param->getValueModulated();
    }
    auto autLane = getRegisteredAutomation(param->idx);
    if (autLane && autLane->isActive()) {
        return param->getValueAutomated();
    }
    return param->getValue();
}

param_unit_t automatable_t::getParamValueDisplay(int32_t idx) {
    auto param = getParam(idx);
    dbgassert(param);
    float val = param->getValue();
    if (param->isModulated()) {
        val = param->getValueModulated();
    }
    auto autLane = getRegisteredAutomation(param->idx);
    if (autLane && autLane->isActive()) {
        val = param->getValueAutomated();
    }
    return convertParamValueToDisplay(param->idx, val);
}

param_unit_t automatable_t::convertParamValueToDisplay(int32_t idx, float value) {
    auto param = getParam(idx);
    dbgassert(param);
    if ((param->idx == PARAM_PAN) || (param->idx >= PARAM_OFFSET_SEND_PAN && param->idx < PARAM_OFFSET_SEND_PAN + MAX_SEND_CHANNELS)) {
        if (value < 0.5)
            return { StringFormat("%.0f", math::clamp((0.5f-value)*2.0f*100.0f, 0.0f, 100.0f)), "L" };
        if (value > 0.5)
            return { StringFormat("%.0f", math::clamp((value-0.5f)*2.0f*100.0f, 0.0f, 100.0f)), "R" };
        return { "", "C" };
    }
    if (param->unit == "dB") {
        float fGain = 1.0f;
        if (dsp_util::getGainLvl(value, fGain)) {
            return {StringFormat("%.3f", dsp_util::dBFS(fGain)), param->unit};
        }
        return {"-INF", param->unit};
    }
    if (param->unit == "%") {
        return {StringFormat("%.3f", value * 100.0f), param->unit};
    }
    return { StringFormat("%f", value), param->unit};
}

param_converted_t automatable_t::convertParamValueDisplay(int32_t idx, const param_unit_t& displayValue) {
    auto param = getParam(idx);
    dbgassert(param);
    //TODO: use std::from_chars when floating point version arrives in libc++
    if ((param->idx == PARAM_PAN) || (param->idx >= PARAM_OFFSET_SEND_PAN && param->idx < PARAM_OFFSET_SEND_PAN + MAX_SEND_CHANNELS)) {

        String str = displayValue.value;
        auto side = 0;
        if (StrUtil::StringReplace(str, "L", "")) {
            side = -1;
        }
        if (StrUtil::StringReplace(str, "R", "")) {
            side = 1;
        }
        if (StrUtil::StringReplace(str, "C", "")) {
            side = 0;
        }
        auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
        auto panPercent = math::clamp((fTextFieldVal/100.0f), 0.0f, 1.0f);
        if (math::roundfS32(panPercent * 100.0f) == 0) {
            side = 0;
        }
        if (side < 0) {
            return { 0.5f - math::clamp(panPercent*0.5f, 0.0f, 0.5f), true };
        }
        if (side > 0) {
            return { 0.5f + math::clamp(panPercent*0.5f, 0.0f, 0.5f), true };
        }
        if (side == 0) {
            return { 0.5f, true };
        }
        return { 0.5f, false };
    }
    auto fTextFieldVal = static_cast<float>(atof(StringAsCStr(displayValue.value)));
    if (param->unit == "dB" && displayValue.unit == "dB") {
        float fGain = dsp_util::fromdBFSClampInf6(fTextFieldVal);
        if (fGain < dsp_util::GAIN_DBFLOOR) {
            fGain = dsp_util::GAIN_DBFLOOR;
        }
        float fNew = dsp_util::clampGain(fGain);
        return {dsp_util::gainToLinScale(fNew), true};
    }
    if (param->unit == "%" && displayValue.unit == "%") {
        return {math::clamp(fTextFieldVal/100.0f, 0.0f, 1.0f), true};
    }
    return {fTextFieldVal, false};
}

bool automatable_t::isParamConnectedTo(int32_t paramIdx, const DAW::modulation_channel_ref& modChannel) const {
    for (auto& input : inputChannelsModulation) {
        if (input.paramIdxDst == paramIdx && input.refSrc == modChannel.refSrc) {
            return true;
        }
    }
    return false;
}

void automatable_t::sampleAutomation(const DAW::Host::PluginManager *const host, int32_t paramIdx, double dTickBegin, double dTickEnd, playback_state state, samplecount_t numSamples, float* buffer) {
    std::vector<int32_t> modulatedParams;
    std::vector<int32_t> processedParamsSorted;
    auto param = getParam(paramIdx);
    if (!assert_expr(param)) {
        std::fill(buffer, buffer + numSamples, 0.0f);
        return;
    }
    std::fill(buffer, buffer + numSamples, param->getValue());
    auto* automation = getRegisteredAutomation(paramIdx);
    if (automation && automation->isActive() && DAW::isPlaybackState(state)) {
        automation->sampleAutomation(dTickBegin, dTickEnd, numSamples, param->getAutomationScale(), buffer);
    } else {
    }
    if (param->isModulated()) {
        auto mods = getModulations(paramIdx);
        for (const auto& mod : *mods) {
            auto ch = DAW::ResolveModulationChannel(host, *mod);
            if (ch && ch->isActive()) {
                ch->sampleAutomation(dTickBegin, dTickEnd, numSamples, mod->scale, buffer);
            }
        }
    }
}

void automatable_t::updateAutomatedParameters(const DAW::Host::PluginManager *const host, tick_t tick, playback_state state) {
    if (automationLanes.empty() && inputChannelsModulation.empty()) {
        return;
    }
    for (auto& entry : mapModulations) {
        int32_t paramIdx = entry.first;
        auto param = getParam(paramIdx);
        auto& modulations = entry.second;
        
        float val = param->getValue();
        auto* autLane = getRegisteredAutomation(paramIdx);
        if (autLane && autLane->isActive() && DAW::isPlaybackState(state)) {
            const auto valAutLane = autLane->modulateValue(tick, val, param->getAutomationScale());
            setAutomatableParam(param, valAutLane, FLG_PAR_UPDATE_AUTOMATED);
            val = valAutLane;
        }
        for (const auto& mod : modulations) {
            auto ch = DAW::ResolveModulationChannel(host, *mod);
            if (ch && ch->isActive()) {
                val = ch->modulateValue(tick, val, mod->scale);
            }
        }
        setAutomatableParam(param, val, FLG_PAR_UPDATE_MODULATED);
    }
    if (DAW::isPlaybackState(state)) {
        for (auto& automLane : automationLanes) {
            if (automLane.isActive()) {
                if (mapModulations.count(automLane.paramIdx) > 0) {
                    continue;
                }
                int32_t paramIdx = automLane.paramIdx;
                auto param = getParam(paramIdx);
                float val = param->getValue();
                auto* autLane = getRegisteredAutomation(paramIdx);
                if (autLane && autLane->isActive() && DAW::isPlaybackState(state)) {
                    const auto valAutLane = autLane->modulateValue(tick, val, param->getAutomationScale());
                    setAutomatableParam(param, valAutLane, FLG_PAR_UPDATE_AUTOMATED);
                    val = valAutLane;
                }
            }
        }
    }
}

float automation_lane_t::modulateValue(tick_t tick, float fIn, const DAW::modulation_scaling_t& scale) const {
    const auto valScaled = scale.min + src.getValueAt(tick) * (scale.max - scale.min);
    switch (scale.mode) {
        case DAW::ModulationMode::ADD:
            fIn += valScaled;
            break;
        case DAW::ModulationMode::MUL:
            fIn *= valScaled;
            break;
        case DAW::ModulationMode::REPLACE:
            fIn = valScaled;
            break;
        default:
            break;
    }
    if (scale.bClamp) {
        fIn = math::clamp(fIn, 0.0f, 1.0f);
    }
    return fIn;
}

void automatable_t::updateModulationMap() {
    std::unordered_map<int32_t, std::vector<DAW::modulation_channel_ref*>> map;
    for (auto& ref : inputChannelsModulation) {
        map[ref.paramIdxDst].push_back(&ref);
    }
    visitParams([&map](auto& param) {
        param.second.setModulated( map.count(param.first) > 0);
    });
    this->mapModulations = std::move(map);
}

automation_lane_t* automatable_t::getOrCreateAutomation(int32_t paramIdx) {
    dbgassert(mapParams.count(paramIdx));
    auto it = std::find_if(automationLanes.begin(), automationLanes.end(), [paramIdx](automation_lane_t& ap) {
        return ap.paramIdx == paramIdx;
    });
    if (it != automationLanes.end()) {
        return &(*it);
    }
    dbgassert(!daw_tls::getTls().mainCtrl || daw_tls::getTls().mainCtrl->getDaw()->getPlayThread()->isLockedOrNotProcessing());
    automationLanes.emplace_back(paramIdx, mapParams[paramIdx].quantizationSteps);
    auto newLane = &automationLanes.back();
    return newLane;
}
void automatable_t::removeModulation(int32_t paramIdx, int32_t modulationIndex) {
    int32_t curIdx = 0;
    for (auto it = inputChannelsModulation.begin(); it != inputChannelsModulation.end(); ++it) {
        if (it->paramIdxDst == paramIdx) {
            if (curIdx++ == modulationIndex) {
                inputChannelsModulation.erase(it);
                updateModulationMap();
                return;
            }
        }
    }
}
void automatable_t::setModulations(const std::vector<DAW::modulation_channel_ref>& inputChannelsModulation) {
    this->inputChannelsModulation = inputChannelsModulation;
    updateModulationMap();
}
