#pragma once
#include <vector>
#include "event.hpp"
#include "str_util.hpp"
#include "math/vec.hpp"

class DropTargetListener {
public:
    virtual ~DropTargetListener() = default;
    virtual bool filesDropBegin(const std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) = 0;
    virtual bool filesDropMove(ivec2 pos, KeyboardMods kbmods)                              = 0;
    virtual void filesDropCancel()                                                 = 0;
    virtual bool filesDropFinal(const std::vector<String>& files, ivec2 pos, KeyboardMods kbmods) = 0;
};
