#pragma once
#include "gui/container/container.h"
class guictr_properties_table : public guictr_base {
public:
    guictr_properties_table() : guictr_base() {
    }
    ~guictr_properties_table() override = default;
    virtual void setDebugPropertyHandle(void* ptr) = 0;
};

void setDebugPropertyHandle(void* ptr);
