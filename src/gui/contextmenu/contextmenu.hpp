#pragma once
#include <vector>
#include "assert_dbg.h"
#include "math/vec.hpp"
#include "event.hpp"
#include "str_util.hpp"
#include "gui/gui.hpp"
#include "guicolors.hpp"
#include "contextmenu_base.hpp"
#include "basectrl.hpp"

class ctxtmenu_entry;
class guictxtmenu : public guictxtmenu_base {
protected:
    std::vector<ctxtmenu_entry*> entries;
    int lvl = 0;

public:
    guictxtmenu();
    ~guictxtmenu() override;
    void setLevel(int _lvl) {
        lvl = _lvl;
    }
    int getLevel() const {
        return lvl;
    }
    void addEntry(ctxtmenu_entry* entry);
    virtual bool clickedElement(ctxtmenu_entry* e, int _id);
    void handleDraggedBegin(MouseEvent& evt) override;
    void layout() override;
    void determineSize(ivec2& prefSize) override;
    void render(NVGcontext* vg) override;
    void setControl(BaseCtrl* parentCtrl) override;
    void closeAllSubmenus();
    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override;
    virtual guictxtmenu* createPopupForEntry(ctxtmenu_entry* entry, int lvl) {
        return nullptr;
    }
};
