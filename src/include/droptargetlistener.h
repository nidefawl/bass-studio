#pragma once
#include <vector>
#include "str_util.h"
#include "math/vec.h"

class DropTargetListener {
public:
    virtual ~DropTargetListener() = default;
    virtual bool filesDropBegin(std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) = 0;
    virtual bool filesDropMove(ivec2 pos, KeyboardMods kbmods)                              = 0;
    virtual void filesDropCancel()                                                 = 0;
    virtual bool filesDropFinal(std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) = 0;
};
