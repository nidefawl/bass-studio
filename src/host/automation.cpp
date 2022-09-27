#include <vector>
#include "automation.h"
#include "config.h"
#include "math/seq_math.h"
#include "plugin/vst_plugin.h"
#include "gui/automation/automatable.h"
#include "str_util.h"
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
void automation_t::sampleAutomation(double dTickBegin, double dTickEnd, samplecount_t numSamples, float* out) const {
    //TODO: write optimal version!
    for (samplecount_t i = 0; i < numSamples; i++) {
        double dTick = dTickBegin + (dTickEnd - dTickBegin) * i / (numSamples - 1);
        *out++ = getValueAtExact(dTick);
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
    float f = math::clamp(1.0f - effect->getParamValue(PARAM_ENABLE), 0.0f, 1.0f);
    if (flags & FLG_PAR_UPDATE_USER) {
        effect->deactivateAutomation(PARAM_ENABLE);
    }
    effect->setParamValue(PARAM_ENABLE, f, flags);
}

void loadAutomation(const std::vector<automation_view_t>& automatedParams, automatable_t* at) {
    if (!automatedParams.empty()) {
        log_lf(Log::L_DEBUG, "Loading %zu automation lanes for device %s\n", automatedParams.size(), StringAsCStr(at->getAutomatableName()));
    }
    at->clearAutomations();
    for (const automation_view_t& automatedParam : automatedParams) {
        int32_t targetParam = automatedParam.targetParam;
        automatable_param_t* paramInstance = at->getParam(targetParam);
        if (!paramInstance) {
            paramInstance = at->getParam(targetParam + PARAM_OFFSET_EXTERNAL);
        }
        if (paramInstance) {
            automation_t* autom = at->getOrCreateAutomation(paramInstance->idx);
            autom->points       = automatedParam.points;
            autom->active       = automatedParam.active;
        } else {
            log_lf(Log::L_WARN, "Param %d missing for device %s\n", automatedParam.targetParam, StringAsCStr(at->getAutomatableName()));
        }
    }
}
void storeAutomation(std::vector<automation_view_t>& automatedParams, automatable_t* at) {
    std::vector<automated_param_t> out;
    at->getAllAutomatedParams(out);
    int total = 0;
    for (const automated_param_t& automatedParam : out) {
        dbgassert(!automatedParam.src.points.empty());
        automation_view_t atv;
        atv.targetParam = automatedParam.paramIdx;
        atv.points      = automatedParam.src.points;
        atv.active      = automatedParam.src.active;
        automatedParams.push_back(atv);
        total++;
    }
    log_printf("Storing %d automation lanes for device %s\n", total, StringAsCStr(at->getAutomatableName()));
}
param_unit_t automatable_t::getParamValueDisplay(int32_t idx) {
    auto param = getParam(idx);
    dbgassert(param);
    return convertParamValueToDisplay(param->idx, param->value);
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
