#pragma once
#include "guicontainer.h"
class debugproperties : public guictr_base {
public:
	debugproperties() : guictr_base() {

	}
	virtual ~debugproperties() {

	}
	virtual void setDebugPropertyHandle(void *ptr) = 0;
};

void setDebugPropertyHandle(void* ptr);
