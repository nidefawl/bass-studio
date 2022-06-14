#pragma once
#include <vector>
#include "math/vec.h"
#include "event.h"
#include "str_util.h"
#include "gui/gui.h"
#include "guicolors.h"
#include "contextmenu_base.h"
#include "basectrl.h"

class ctxtmenu_entry;
class guictxtmenu : public guictxtmenu_base {
protected:
    std::vector<ctxtmenu_entry*> entries;

public:
    guictxtmenu() : guictxtmenu_base() {
        setCanMouseHit(true);
        setBackgroundRendered(true);
        setBackgroundRenderedInset(false);
        setSnapSides(ivec4(1));
    }
    ~guictxtmenu() override {
        for (ctxtmenu_entry* e : entries) {
            delete e;
        }
    }
    void addEntry(ctxtmenu_entry* entry) {
        size.x = math::max(size.x, entry->width);
        entries.push_back(entry);
        entry->theme = theme;
    }
    virtual void clicked(int _id) {
        closeContextMenu();
    }
    virtual void clickedElement(ctxtmenu_entry* e, int _id) {
        clicked(_id);
    }
    void handleDraggedBegin(MouseEvent& evt) override {
        ivec2 local = evt.relMousepos;
        for (ctxtmenu_entry* e : entries) {
            if (pos.y+e->y > parent->size.y)
                break;
            if (pos.y+e->y+e->height<0)
                continue;
            int n = e->getClicked(size, local);
            if (n >= 0) {
                clickedElement(e, n);
                return;
            }
        }
    }
    void layout() override {
        determine_string_width strw(parentCtrl, theme);
        int y = paddingV;
        for (ctxtmenu_entry* e : entries) {
            e->layout(size, fontSize, strw);
            e->y = y;
            y += e->height + paddingV;
        }
    }
    void determineSize(ivec2& prefSize) override {
        ivec2 newMaxSize = { size.x, paddingV };
        for (ctxtmenu_entry* e : entries) {
            newMaxSize.x = math::max(newMaxSize.x, e->width);
            newMaxSize.y += e->height + paddingV;
        }
        if (entries.empty()) {
            newMaxSize.y += paddingV;
        }
        prefSize = newMaxSize;
    }

    void render(NVGcontext* vg) override {
        if (isBackgroundRendered()) {
            renderBackground(vg);
        }
        setScissorTransform(vg);
        int idx     = 0;
        ivec2 mouse = parentCtrl->m_mousePos;
        mouse       = toContainerSpace(mouse);
        for (ctxtmenu_entry* e : entries) {
            if (pos.y+e->y > parent->size.y)
                break;
            if (pos.y+e->y+e->height<0)
                continue;
            e->render(size, vg, idx, mouse);
            idx++;
        }
    }
    void setControl(BaseCtrl* parentCtrl) override {
        guictxtmenu_base::setControl(parentCtrl);
        for (auto* g : entries) {
            g->theme = parentCtrl ? parentCtrl->getTheme() : nullptr;
        }
    }
};
