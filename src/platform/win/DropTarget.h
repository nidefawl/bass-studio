#pragma once

#include <windows.h>

#include <vector>
#include "droptargetlistener.h"


class DropTargetImpl;
class DropTarget
{
public:
	DropTarget(DropTargetImpl* _impl)
	: impl(_impl)
	{

	}
	DropTargetImpl* const impl;

};
DropTarget *RegisterDropWindow(HWND hwnd, DropTargetListener *dropTargetListener);
void UnregisterDropWindow(HWND hwnd, DropTarget *pDropTarget);
