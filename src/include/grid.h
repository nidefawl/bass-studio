#pragma once
#include "math/vec.h"
#include "types.h"
#include <vector>
#include "math/seq_math.h"
#include "seq_time.h"
#include "guicolors.h"
#include "logging.h"
#include "layout.h"
#include "grid_constants.h"

struct grid_div {
    tick_t time;
    beatbar16th_t pos;
    double screenpos;
    int8_t color;
    float thickness;
    double width;
};
struct grid_density {
    bool enabled          = true;
    bool isfixed          = false;
    int8_t fixedBars      = 2;
    int8_t dynamicDensity = 3;
    template<class Archive>
    void serialize(Archive& ar) {
        ar(enabled, isfixed, fixedBars, dynamicDensity);
    }
    int getSnap()  const {
        if (!enabled) {
            return GRID_OFF;
        }
        if (isfixed)
            return fixedBars;
        return dynamicDensity;
    }
};
class scaled_grid;
class grid_changed_cb {
public:
    virtual ~grid_changed_cb() = default;
    virtual void gridChanged(scaled_grid& grid) = 0;
};

class scaled_grid final : public layout_grid_t {
protected:
    std::vector<grid_changed_cb*> callbacks;
    int lastOffset  = 0;
    double lastZoom = DEFAULT_ZOOM;
    int lastW       = 200;
    bool dirty      = false;
    uint8_t gridMaxDens = 8;
public:
    grid_density grid_dens;
    std::vector<grid_div> gridList;

    double bars     = 0;
    double bar_size = 0;
    double incr_bg  = 0;

public:
    scaled_grid() {
        showRange(0, TICKS_BAR * 4);
    }
    void setGridMaxDens(uint8_t dens) {
        gridMaxDens = math::clamp<uint8_t>(dens, 1, 8);
    }
    void addCallback(grid_changed_cb* cb) {
        this->callbacks.push_back(cb);
    }
    void setLayout(layout_grid_t& layout) {
        setZoom(layout.zoom);
        setOffset(layout.offset);
        notifyChange();
    }
    void update(ivec2 contentsize) {
        dirty = false;
        calcLen(offset, zoom, contentsize.x);
        lastOffset = offset;
        lastZoom   = zoom;
        lastW      = contentsize.x;
    }
    tick_t getTickLength() const;
    void notifyChange();
    int32_t getOffset() const {
        return this->offset;
    }
    double getZoom() const {
        return this->zoom;
    }
    void setZoom(double zoom);
    void setOffset(int newOffset);
    double toObjSpace(double screenx) const {
        return toObjSpace(screenx, this->zoom, this->offset);
    }
    double toObjSpace(double screenx, double _zoom, double _offset) const {
        return _zoom * (screenx + _offset) / 8.0;
    }
    double toObjSpace0Offset(double screenx) const {
        return toObjSpace(screenx, this->zoom, 0.0);
    }
    double toScreenSpace(double objx) const {
        return toScreenSpace(objx, this->zoom);
    }
    double toScreenSpace(double objx, double _zoom) const {
        return 8.0 * objx / _zoom;
    }
    double calcOffset(double screenx, double objx) const {
        double screenpos = toScreenSpace(objx);
        return screenpos - screenx;
    }
    tick_t screenToTickSnap(int32_t x, int snap) const;
    tick_t screenToTickSnapExact(double x, int snap) const;
    tick_t tickSnapExact(tick_t x, int snap) const;

    tick_t screenToTick(double x) const {
        double d     = toObjSpace(x);
        double dTick = TICKS_BAR * d;
        return (int32_t) math::rounddS32(dTick);
    }
    double screenToTickD(double x) const {
        double d     = toObjSpace(x);
        double dTick = TICKS_BAR * d;
        return dTick;
    }

    double tickToScreenD(double x) const {
        double bar            = x / (double) TICKS_BAR;
        double objspaceOffset = toObjSpace(0, this->zoom, offset);
        return toScreenSpace(bar - objspaceOffset);
    }
    double tickLenToScreen(double x) const{
        double bar = x / (double) TICKS_BAR;
        return toScreenSpace(bar);
    }
    int32_t pixelsToTicks(int32_t pixels) const {
        double x = toObjSpace(pixels, this->zoom, 0) * TICKS_BAR;
        return math::max(1, math::ceildS32(x));
    }
    int32_t pixelsToTicks2(int32_t pixels) const {
        double x = toObjSpace(pixels, this->zoom, 0) * TICKS_BAR;
        return math::rounddS32(x);
    }
    void showRange(tick_t start, tick_t end);
    void calcLen(int scrollOffsetX, double zoom, int contentWidth);
    void makeTickVisible(tick_t tickTime);
    tick_t prev(tick_t tickTime) const;
    tick_t next(tick_t tickTime) const;
    tick_t distPrev(tick_t tickTime) const;
    tick_t distNext(tick_t tickTime) const;
};
