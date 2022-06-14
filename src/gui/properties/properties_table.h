#pragma once
#include "gui/container/container.h"
class guictr_properties_table : public guictr_base {
public:
    guictr_properties_table() : guictr_base() {
        setBackgroundRendered(false);
    }
    ~guictr_properties_table() override = default;
    virtual void setDebugPropertyHandle(void* ptr) = 0;
    GuiColor::constant_t getBackgroundColorFromState(int32_t stateflags) const override {
        if (focused()) {
            return GuiColor::COL_BG_BRT;
        }
        return GuiColor::COL_BG_BRT;
    }

    GuiColor::constant_t getOuterBackgroundColorFromState(int32_t stateflags) const override {
        if (focused()) {
            return GuiColor::COL_BG_BRT;
        }
        return GuiColor::COL_BG_BRT;
    }
};

void setGlobalDebugPropertyHandle(void* ptr);
