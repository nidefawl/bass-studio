#pragma once
#include "math/vec.h"
#include "math/seq_math.h"
#include "gui.h"
#include "exceptions.h"
#include "mouse.h"
#include "event.h"
#include "basectrl.h"
#include "table.h"

class Splitter;
class splitter_cb {
protected:
    ~splitter_cb() = default;
public:
    virtual void handleSplitterChanged(Splitter& splitter, float scale, int clampedAt) = 0;
    virtual ivec2 getContainerSize()                                                   = 0;
};

class Splitter : public guictr_base {
    int type;
    float scale;
    float scaleMin, scaleMax;
    splitter_cb* notifyCtrl = nullptr;
public:
    static constexpr int SPLITTER_LAYOUT_THICKNESS = 10;

    Splitter(int _type, float _scale)
        : guictr_base(),
          type(_type),
          scale(_scale),
          scaleMin(0),
          scaleMax(1) {
        padding  = 0;
    }
    void setMinMax(float _min, float _max) {
        this->scaleMin = _min;
        this->scaleMax = _max;
    }
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
        if (this->contains(mpos) /*&& evt.type <= MouseHitType::MOUSE_RIGHT*/) {
            evt.requestFocus(this);
            evt.requestCursor(type == 0 ? CURSOR_RESIZE_V : CURSOR_RESIZE_H);
            return true;
        }
        return false;
    }
    int32_t leftOrTop(int32_t wh) const {
        return math::roundfS32(wh * scale);
    }
    int32_t rightOrBottom(int32_t wh) const {
        return wh - leftOrTop(wh);
    }
    void handleDraggedBegin(MouseEvent& evt) override {
    }
    void handleDraggedMove(MouseEvent& evt) override;
    void handleDraggedRelease(MouseEvent& evt) override {
    }
    bool isStaticContainer() override {
        return true;
    }
    float getScale() const {
        return this->scale;
    }
    void setScale(float f) {
        this->scale = f;
    }
    float getMin() const {
        return scaleMin;
    }
    float getMax() const {
        return scaleMax;
    }
    void render(NVGcontext* vg) override;

    void addProperties(Table::tbl* table) override;
    void setCallback(splitter_cb *splitterCb) {
        this->notifyCtrl = splitterCb;
    }
    void setParent(guibase *parent) override {
        dbgassert(this->notifyCtrl);
        guictr_base::setParent(parent);
    }
    int getType() const {
        return type;
    }
};
