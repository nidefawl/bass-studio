#pragma once
#include "guicontainer.h"
class debugproperties : public guictr_base {
public:
    debugproperties() : guictr_base() {
    }
    ~debugproperties() override = default;
    virtual void setDebugPropertyHandle(void* ptr) = 0;
};

void setDebugPropertyHandle(void* ptr);
