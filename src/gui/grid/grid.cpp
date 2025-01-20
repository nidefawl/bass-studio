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

tick_t scaled_grid::getTickLength() const {
    if (this->gridList.size() < 2) { return TICKS_BAR; }

    //TODO: way too hacky
    auto first  = this->gridList[0];
    auto second = this->gridList[1];
    return second.time - first.time;
}
void scaled_grid::setOffset(double newOffset) {
    newOffset = newOffset < 0 ? 0 : newOffset;
    this->offset = newOffset;
}
tick_t scaled_grid::next(tick_t tick) const {
    tick_t len = getTickLength();
    tick_t pos = floor(tick / (double) len);
    pos++;
    return pos * len;
}
tick_t scaled_grid::prev(tick_t tick) const {
    tick_t len = getTickLength();
    tick_t pos = floor(tick / (double) len);
    if (pos * len == tick) { pos--; }
    return pos * len;
}
tick_t scaled_grid::distNext(tick_t tick) const {
    tick_t len = getTickLength();
    tick_t pos = floor(tick / (double) len);
    pos++;
    return (pos * len) - tick;
}
tick_t scaled_grid::distPrev(tick_t tick) const {
    tick_t len = getTickLength();
    tick_t pos = floor(tick / (double) len);
    if (pos * len == tick) { pos--; }
    return (pos * len) - tick;
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
    using float_type = decltype(grid_div::screenpos);
    gridList.clear();
    gridList.reserve(100);
    const float_type stepSize = fzoom * 128;
    const float_type scale    = 1024.0;
    const float_type barSize  = scale / stepSize;
    //bg
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


    //grid2
    {
        int denum_step        = 4;
        int step              = 1;
        int denum_substep     = 0;
        float_type minBarSize = 12;
        float_type minSubSize = 12;
        //        grid_dens.isfixed        = false;
        //        grid_dens.fixedBars      = 6;
        //        grid_dens.dynamicDensity = 3;
        if (grid_dens.isfixed) {
            if (grid_dens.fixedBars < 4) {
                step       = (1 << (3 - grid_dens.fixedBars));
                denum_step = 0;
            } else {
                denum_step = (1 << (grid_dens.fixedBars - 3));
                while (denum_step > 4) {
                    denum_step /= 2;
                    if (denum_substep == 0) denum_substep = 2;
                    else
                        denum_substep *= 2;
                }
            }
        } else {
            step          = 1;
            denum_substep = 32;
            minBarSize    = (float_type) (1 << (this->gridMaxDens - math::min<uint8_t>(grid_dens.dynamicDensity, this->gridMaxDens)));
            minSubSize    = (float_type) (1 << (this->gridMaxDens - math::min<uint8_t>(grid_dens.dynamicDensity, this->gridMaxDens)));
        }
        while (step < (1 << 14) && barSize * step < minBarSize) {
            step = step * 2;
            //            substep = 0;
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
        //TODO: very important: make sure grid is never empty
        auto* project = project_controller_t::get();
        for (int bar = 0; bar < numBarsOnScreen; bar += step) {
            const float_type pos = bar_offset + bar * barSize;
            const tick_t timeBar = (firstBarLeftOfScreen + bar) * TICKS_BAR;

            grid_div div  = {};
            div.time      = timeBar;
            div.pos       = project->toBeatBar16th(timeBar, false);
            div.screenpos = pos;
            div.width     = denum_step > 0 ? (denum_substep > 0 ? denom_sub_size : denom_size) : barSize * step;
            div.color     = 0;
            div.thickness = 0.9f;
            gridList.push_back(div);
            for (int bar_denom = 0; bar_denom < denum_step; bar_denom++) {
                const tick_t timeQuarter = timeBar + (bar_denom * (4 / denum_step)) * TICKS_QUARTER;

                const float_type pos_denom = pos + bar_denom * denom_size;
                if (bar_denom > 0) {
                    grid_div div_quarter  = {};
                    div_quarter.time      = timeQuarter;
                    div_quarter.pos       = project->toBeatBar16th(timeQuarter, false);
                    div_quarter.screenpos = pos_denom;
                    div_quarter.width     = denum_substep > 0 ? denom_sub_size : denom_size;
                    div_quarter.color     = 1;
                    div_quarter.thickness = 0.75f;
                    gridList.push_back(div_quarter);
                }
                for (int bar_denom_sub = 1; bar_denom_sub < denum_substep; bar_denom_sub++) {
                    const tick_t timeSmall = timeQuarter + (bar_denom_sub * (32 / denum_substep)) * (TICKS_16TH >> 3);

                    grid_div div_smaller  = {};
                    div_smaller.time      = timeSmall;
                    div_smaller.pos       = project->toBeatBar16th(timeSmall, false);
                    div_smaller.screenpos = bar_offset + bar * barSize + bar_denom * denom_size + bar_denom_sub * denom_sub_size;
                    div_smaller.width     = denom_sub_size;
                    div_smaller.color     = 2;
                    div_smaller.thickness = 0.6f;
                    gridList.push_back(div_smaller);
                }
            }
        }
        this->bars = contentWidth / barSize;
    }

    this->bar_size = barSize;
}
