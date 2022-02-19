#pragma once

#include <Windows.h>

#include <vector>
#include "droptargetlistener.h"


class DropTargetImpl;
class DropTarget {
public:
    explicit DropTarget(DropTargetImpl* _impl)
        : impl(_impl) {
    }
    DropTargetImpl* const impl;
};
DropTarget* RegisterDropWindow(HWND hwnd, DropTargetListener* dropTargetListener);
void UnregisterDropWindow(HWND hwnd, DropTarget* pDropTarget);
