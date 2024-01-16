
#include "gui/views/controls.h"
#include "types.h"
#include "str_util.h"
#include "basectrl.h"
#include "gui/container/container.h"
#include "logging.h"

namespace {
class guictr_bla : public guictr_base {
public:
    guictr_bla()
        : guictr_base()
    {
        setLayoutMode(autolayout_mode::LAYOUT_VERTICAL);
    }
    ~guictr_bla() override {
        removeGuis();
    }
};
}
class guictr_test2 : public guictr_base {
    guictr_bla ctr;
public:
    guictr_test2();
    ~guictr_test2() override;
    void layout() override;
};

guictr_test2::guictr_test2() : guictr_base() {
    setBackgroundRendered(true);
    add(&ctr);
}
guictr_test2::~guictr_test2() {
    removeGuis();
}

void guictr_test2::layout() {
    ivec2 cs          = getSizeContent();
    ctr.pos = {};
    ctr.size = cs;
    for (auto* gui : guis) {
        gui->layout();
    }
}

guictr_base* makeGuiTestCtr2() {
    return new guictr_test2();
}
