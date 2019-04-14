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
	~guidialog_base() { }
	void determineSize(ivec2& prefSize) override {
		prefSize = dialogSize;
	}
	virtual bool isDialog() {
		return true;
	}
};
class guidialog_about : public guidialog_base {
	guibutton btnClose;
public:
	guidialog_about();
	~guidialog_about() {
		removeGuis();
	}
	void render(NVGcontext* vg) override;
	void layout() override;
	void buttonClicked(guibase* button) override;
};
