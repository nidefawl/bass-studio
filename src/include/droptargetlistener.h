#pragma once
#include <vector>
#include "event.h"
#include "str_util.h"
#include "math/vec.h"

class DropTargetListener {
public:
    virtual ~DropTargetListener() = default;
    virtual bool filesDropBegin(const std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) = 0;
    virtual bool filesDropMove(ivec2 pos, KeyboardMods kbmods)                              = 0;
    virtual void filesDropCancel()                                                 = 0;
    virtual bool filesDropFinal(const std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) = 0;
};
