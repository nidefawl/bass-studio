#include <algorithm>
#include "guicolors.h"
#include "guiconstant.h"
#include "host/mainctrl.h"
#include "math/vec.h"
#include "gui.h"
#include "button.h"
#include "guicontextmenu_base.h"
#include "platform.h"


class gui_autosave : public guictxtmenu_base {
protected:
    bool hadMouseFocus = false;
    guibutton btnSaveNow;
    int64_t tmDelay  = 0L;
    int64_t tmCreate = 0L;
    String str;
    String str2;

public:
    gui_autosave() : guictxtmenu_base() {
        setCanMouseHit(true);
        setBackgroundRendered(false);
        setBackgroundRenderedInset(false);
        setSnapSides(ivec4(1));
        add(&btnSaveNow);
        setBackgroundRendered(true);

//        setBackgroundRenderedInset(false);
//        setSnapSides(ivec4(1));
        padding = 0;
        margin  = 0;
        //margin *= 2;
        //determineSize(size);
        //maxHeight = size.y;
        canTakeInputFocus = true;
    }
    ~gui_autosave() override {
        removeGuis();
    }
    void setDelay(int64_t _tmDelay) {
        this->tmDelay  = _tmDelay;
        this->tmCreate = getTimeMillis();
    }
//    bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
//        if (contains(mpos)) {
//            if (evt.type == MouseHitType::MOUSE_LEFT || evt.type == MouseHitType::MOUSE_RIGHT)
//                hadMouseFocus = true;
//            evt.requestFocus(this);
//            return true;
//        }
//        return false;
//    }
    bool isTransient() override {
//        return !hadMouseFocus;
        return false;
    }
    void onTick(AppCtrl* appctrl) override {
        layout();
        auto tmLeft  = math::max<int64_t>(0, this->tmDelay - (getTimeMillis() - this->tmCreate));
        String fname = DawInstance::get()->getAutoSaveFilename();
        str          = StringFormat("Autosave as %s", StringAsCStr(fname));
        str2         = StringFormat("in %d seconds", tmLeft / 1000);
        btnSaveNow.setText("Save now");
        if (tmLeft <= 0) {
            closeContextMenu();
            DawInstance::get()->triggerAutoSave();
        }
    }

    void buttonClicked(guibase* button) override {
        closeContextMenu();
        DawInstance::get()->triggerAutoSave();
    }
    void layout() override {
        btnSaveNow.pos  = ivec2(size.x - size.x / 4, 0);
        btnSaveNow.size = ivec2(size.x / 4, size.y);
        for (auto* g : guis) {
            g->layout();
        }
    }
    void render(NVGcontext* vg) override {
        nvgSave(vg);
        guictxtmenu_base::render(vg);
        nvgRestore(vg);
        if (str.length()) {
            nvgSave(vg);
            setScissorTransform(vg);

            ivec2 renderPos(0);
            if (str.length() > 0) {
                float fFontScale = 1.0f;
                ivec2 size       = this->size;
                size.x           = size.x - btnSaveNow.size.x;
                int fontScale    = math::roundfS32((this->fontSize > 0 ? this->fontSize : math::min(size.y, size.x)) * fFontScale);
                auto fontColor   = (getStateFlags() & FLG_ENBL) ? GuiColor::COL_LABEL_ACTIVE : GuiColor::COL_LABEL_INACTIVE;
                UTIL_setFont(vg, theme, fontScale, theme->getContrastColor(GuiColor::COL_CTXTMNU_BG), NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
                renderCenteredMultilineText(vg, theme, str + "\n" + str2, fontScale, fontColor, renderPos, size);
            }
            nvgRestore(vg);
        }
    }
};


guictxtmenu_base* makeGuiAutosave(int64_t delay) {
    gui_autosave* gui = new gui_autosave();
    gui->setDelay(delay);
    return gui;
}
