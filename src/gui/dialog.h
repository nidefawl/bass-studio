#pragma once
#include "math/vec.h"
#include "str_util.h"
#include "knob.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "button.h"
#include <vector>

class guidialog_base : public guictxtmenu_base {
	const ivec2 dialogSize;
public:
	guidialog_base(ivec2 _dialogSize) : dialogSize(_dialogSize) {
		setCanMouseHit(true);
		padding = CONTENT_INSET;
		margin = CTR_SPACING;
		margin *= 2;
		determineSize(size);
		maxHeight = size.y;
		canTakeInputFocus = true;
	}
	~guidialog_base() { assert(guis.empty()); }
	void determineSize(ivec2& prefSize) override {
		prefSize = dialogSize;
	}
	virtual bool isDialog() {
		return true;
	}
};
