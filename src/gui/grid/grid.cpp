#include "assert_dbg.h"
#include "math/seq_math.hpp"
#include "seq_time.hpp"
#include "grid.hpp"
#include "host/project/projectcontroller.hpp"

void scaled_grid::makeTickVisible(tick_t tickTime) {
    double tickBars = tickTime / (double) TICKS_BAR;
    if (lastW > 0) {
        double screenx = toScreenSpace(tickBars);
        if (screenx < this->offset) { setOffset((int) screenx); }
        if (screenx > this->offset + lastW) { setOffset((int) (screenx - (lastW))); }
    }
    notifyChange();
}
void scaled_grid::showRange(tick_t start, tick_t end) {
    double rangeBars = (end - start) / (double) TICKS_BAR;
    if (lastW > 0) {
        this->zoom       = (float) (8.0f / (lastW / rangeBars));
        this->offset     = toScreenSpace(start / (double) TICKS_BAR);
        notifyChange();
    }
}
void scaled_grid::setZoom(double fNewZoom) {
    double newZoom            = math::clamp(fNewZoom, MIN_ZOOM, MAX_ZOOM);
    double length             = toObjSpace(lastW, fNewZoom, 0);
    double projectWorkingArea = project_controller_t::get()->getProjectWorkingArea();
    if (length > projectWorkingArea) {
        newZoom = 8.0 / (lastW / projectWorkingArea);
    }
    this->zoom = math::clamp(newZoom, MIN_ZOOM, MAX_ZOOM);
}
void scaled_grid::notifyChange() {
    for (grid_changed_cb* cb : this->callbacks) { cb->gridChanged(*this); }
}

double scaled_grid::getTickLength() const {
    auto& gridList = grid_dens.triplets ? this->gridListTriplets : this->gridList;
    if (gridList.size() < 2) { return stepWidthTicks; }

    //TODO: way too hacky
    auto first  = gridList[0];
    auto second = gridList[1];
    return second.dTime - first.dTime;
}
void scaled_grid::setOffset(double newOffset) {
    newOffset = newOffset < 0 ? 0 : newOffset;
    this->offset = newOffset;
}
tick_t scaled_grid::next(tick_t tick) const {
    auto len = getTickLength();
    tick_t pos = math::floordS32(tick / len);
    pos++;
    return math::floordS32(pos * len);
}
tick_t scaled_grid::prev(tick_t tick) const {
    auto len = getTickLength();
    tick_t pos = math::floordS32(tick / len);
    if (pos * len == tick) { pos--; }
    return math::floordS32(pos * len);
}
tick_t scaled_grid::distNext(tick_t tick) const {
    auto len = getTickLength();
    tick_t pos = math::floordS32(tick / len);
    pos++;
    return math::floordS32(pos * len) - tick;
}
tick_t scaled_grid::distPrev(tick_t tick) const {
    auto len = getTickLength();
    tick_t pos = math::floordS32(tick / len);
    if (pos * len == tick) { pos--; }
    return math::floordS32(pos * len) - tick;
}
tick_t scaled_grid::screenToTickSnapExact(double x, int snap) const {
    return tickSnapExact(screenToTick(x), snap);
}
tick_t scaled_grid::tickSnapExact(tick_t tick, int snap) const {
    if ((snap & SNAP_ON) && this->grid_dens.getSnap() != GRID_OFF) {
        tick_t p = math::abs(distPrev(tick));
        tick_t n = math::abs(distNext(tick));
        auto closestTick = p < n ? prev(tick) : next(tick);
        return (snap & SNAP_UNCLAMPED_ZERO) ? closestTick : math::max<tick_t>(0, closestTick);
    }
    return tick;
}
tick_t scaled_grid::screenToTickSnap(int32_t x, int snap) const {
    return screenToTickSnapExact(x, snap);
}

void scaled_grid::calcLen(int scrollOffsetX, double fzoom, int contentWidth) {
    //TODO: very important: make sure grid is never empty
    auto* project = project_controller_t::get();
    using float_type = decltype(grid_div::screenpos);
    const float_type stepSize = fzoom * 128;
    const float_type scale    = 1024.0;
    const float_type barSize  = scale / stepSize;
    // bg
    {
        float_type steps_bg = stepSize;
        float_type incr     = scale / steps_bg;
        float_type minW     = 256.0;
        float_type maxW     = minW * 2.0;
        while (incr < minW) {
            steps_bg /= 4;
            incr = scale / steps_bg;
        }
        if (incr > maxW) {
            steps_bg *= 4;
            incr = scale / steps_bg;
        }
        this->incr_bg = incr;
    }

    this->bars = contentWidth / barSize;
    this->bar_size = barSize;

    // grid
    for (int pass = 0; pass < 2; ++pass)
    {
        auto& gridList = pass == 0 ? this->gridList : gridListTriplets;
        auto& gridListTicks = pass == 0 ? this->gridListTicks : gridListTicksTriplets;
        gridList.clear();
        gridList.reserve(100);
        gridListTicks.clear();
        int quarterOrTriples  = pass == 0 ? 4 : 3;
        int denum_step        = quarterOrTriples;
        int step              = 1;
        int denum_substep     = 0;
        float_type minBarSize = 12;
        float_type minSubSize = 12;
        if (grid_dens.isfixed) {
            if (grid_dens.fixedBars < 4) {
                step       = (1 << (3 - grid_dens.fixedBars));
                denum_step = 0;
            } else {
                if (pass == 0) {
                    denum_step = (1 << (grid_dens.fixedBars - 3));
                    while (denum_step > quarterOrTriples) {
                        denum_step /= 2;
                        if (denum_substep == 0) denum_substep = 2;
                        else denum_substep *= 2;
                    }
                } else { // triplets
                    denum_step = 3;
                    while (denum_step < (1 << (grid_dens.fixedBars - 3))) {
                        denum_step *= 2;
                    }
                }
            }
        } else {
            step          = 1;
            denum_substep = pass == 0 ? 32 : 24;
            minBarSize    = (float_type) (1 << (this->gridMaxDens - math::min<uint8_t>(grid_dens.dynamicDensity, this->gridMaxDens)));
            minSubSize    = (float_type) (1 << (this->gridMaxDens - math::min<uint8_t>(grid_dens.dynamicDensity, this->gridMaxDens)));
        }
        while (step < (1 << 14) && barSize * step < minBarSize) {
            step = step * 2;
        }
        float_type denom_size = denum_step > 0 ? barSize / (float_type) denum_step : 0;
        while (denum_step > 0 && denom_size < minSubSize) {
            denum_step = denum_step / 2;
            denom_size = barSize / denum_step;
        }
        if (denum_step == 0) { denum_substep = 0; }
        float_type denom_sub_size = denum_substep > 0 ? denom_size / (float_type) denum_substep : 0;
        while (denum_substep > 0 && denom_sub_size < minSubSize) {
            denum_substep  = denum_substep / 2;
            denom_sub_size = denom_size / denum_substep;
        }
        float_type bar_offset    = -fmod((double) offset, (double) (barSize * step));
        int numBarsOnScreen      = math::ceildS32(contentWidth / barSize) + 1;//todo: maybe make this optimal
        int firstBarLeftOfScreen = math::floordS32(offset / (barSize));
        firstBarLeftOfScreen     = (firstBarLeftOfScreen / step) * step;
        
        if (pass == 1 && this->grid_dens.triplets && denum_substep > 1) {
            this->incr_bg *= 4;
            this->incr_bg /= 3;
        }
        double minStep = TICKS_BAR;
        for (int bar = 0; bar < numBarsOnScreen; bar += step) {
            const float_type pos = bar_offset + bar * barSize;
            const auto timeBar_i64 = (int64_t(firstBarLeftOfScreen) + bar) * TICKS_BAR;
            if (timeBar_i64 < std::numeric_limits<int32_t>::min()) {
                continue; // skip bars that are out of range
            }
            if (timeBar_i64 > std::numeric_limits<int32_t>::max()) {
                continue; // skip bars that are out of range
            }
            const tick_t timeBar = tick_t(timeBar_i64);
            grid_div div  = {};
            div.time      = timeBar;
            div.dTime     = timeBar;
            div.pos       = project->toBeatBar16th(timeBar, false);
            div.screenpos = pos;
            div.width     = denum_step > 0 ? (denum_substep > 0 ? denom_sub_size : denom_size) : barSize * step;
            div.color     = 0;
            div.thickness = 0.9f;
            // assert that timeBar is > last time
            dbgassert(gridListTicks.empty() || timeBar > gridListTicks.back());
            gridListTicks.push_back(timeBar);
            gridList.push_back(div);
            const auto ticksBar = tick_t(TICKS_BAR);
            for (int bar_denom = 0; bar_denom < denum_step; bar_denom++) {
                const auto denum_step_ticks = ticksBar / double(denum_step);
                minStep = denum_step_ticks;
                const auto timeQuarterDouble = timeBar_i64 + double(bar_denom * ticksBar) / double(denum_step);
                if (timeQuarterDouble < std::numeric_limits<int32_t>::min()) {
                    continue; // skip denoms that are out of range
                }
                if (timeQuarterDouble > std::numeric_limits<int32_t>::max()) {
                    continue; // skip denoms that are out of range
                }
                const auto timeQuarter = math::rounddS32(timeQuarterDouble);

                const float_type pos_denom = pos + bar_denom * denom_size;
                if (bar_denom > 0) {
                    grid_div div_quarter  = {};
                    div_quarter.time      = timeQuarter;
                    div_quarter.dTime     = timeQuarterDouble;
                    div_quarter.pos       = project->toBeatBar16th(timeQuarter, false);
                    div_quarter.screenpos = pos_denom;
                    div_quarter.width     = denum_substep > 0 ? denom_sub_size : denom_size;
                    div_quarter.color     = 1;
                    div_quarter.thickness = 0.75f;
                    dbgassert(gridListTicks.empty() || timeQuarter > gridListTicks.back());
                    gridListTicks.push_back(timeQuarter);
                    gridList.push_back(div_quarter);
                }
                for (int bar_denom_sub = 1; bar_denom_sub < denum_substep; bar_denom_sub++) {
                    const auto timeNthDouble = timeQuarterDouble + double(bar_denom_sub * denum_step_ticks) / denum_substep;
                    if (timeNthDouble < std::numeric_limits<int32_t>::min()) {
                        continue; // skip nth that are out of range
                    }
                    if (timeNthDouble > std::numeric_limits<int32_t>::max()) {
                        continue; // skip nth that are out of range
                    }
                    const auto timeNth = math::rounddS32(timeNthDouble);
                    minStep = denum_step_ticks / denum_substep;

                    grid_div div_smaller  = {};
                    div_smaller.time      = timeNth;
                    div_smaller.dTime     = timeNthDouble;
                    div_smaller.pos       = project->toBeatBar16th(timeNth, false);
                    div_smaller.screenpos = pos_denom + bar_denom_sub * denom_sub_size;
                    div_smaller.width     = denom_sub_size;
                    div_smaller.color     = 2;
                    div_smaller.thickness = 0.6f;
                    dbgassert(gridListTicks.empty() || timeNth > gridListTicks.back());
                    gridListTicks.push_back(timeNth);
                    gridList.push_back(div_smaller);
                }
            }
        }

        if ((pass != 0) == this->grid_dens.triplets) {
            this->stepWidthTicks = minStep;
        }
    }
}
