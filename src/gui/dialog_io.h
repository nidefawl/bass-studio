#pragma once
#include "button.h"
#include "dialog.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "host/mainctrl.h"
#include "knob.h"
#include "math/vec.h"
#include "str_util.h"
#include <vector>

class gui_list;
class guidropdownbase;

namespace DAW::DialogSettings {
    
class setting_dialog : public guictr_base {
public:
    virtual void onDialogShow() = 0;
};

class guidialog_settings : public guidialog_base {
    struct dialog_entry;
    std::vector<dialog_entry*> entries;
    dialog_entry* activeEntry = nullptr;
    guibutton btnClose;
    void init(DawInstance* daw);

public:
    explicit guidialog_settings(DawInstance* daw);
    ~guidialog_settings() override;
    void render(NVGcontext* vg) override;
    void layout() override;
    void buttonClicked(guibase* button) override;
    void setActiveEntry(int32_t idx);
    void addEntry(setting_dialog* ctr, String title);
};

}
