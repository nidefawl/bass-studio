#pragma once
#include "math/vec.h"
#include "str_util.h"
#include "knob.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "button.h"
#include <vector>

class guidialog_base : public guictxtmenu_base {
protected:
    const ivec2 dialogSize;
    const bool resizeable;

public:
    guidialog_base(ivec2 _dialogSize, bool _resizeable = false) : dialogSize(_dialogSize), resizeable(_resizeable) {
        setCanMouseHit(true);
        padding = CONTENT_INSET;
        margin  = CTR_SPACING;
        margin *= 2;
        determineSize(size);
        maxHeight         = size.y;
        canTakeInputFocus = true;
    }
    ~guidialog_base() override { dbgassert(guis.empty()); }
    void determineSize(ivec2& prefSize) override {
        if (resizeable) {
            prefSize = math::maxvec2(dialogSize, prefSize);
        } else {
            prefSize = dialogSize;
        }
    }
    bool isDialog() override {
        return true;
    }
    bool isDialogResizeable() const {
        return resizeable;
    }
};
