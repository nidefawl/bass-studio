#pragma once
#include "gui/controls/button.h"
#include "dialog.h"
#include "gui/container/container.h"
#include "gui/contextmenu/contextmenu_base.h"
#include "host/daw/mainctrl.h"
#include "gui/controls/knob.h"
#include "math/vec.h"
#include "str_util.h"
#include <vector>

class gui_list;
class guidropdownbase;

namespace DAW::DialogSettings {
    
class setting_dialog : public guictr_base {
public:
    setting_dialog() : guictr_base() {
        setBackgroundRendered(false);
        setBackgroundRenderedInset(false);
        setCanMouseHit(true);
        padding = 0;
        margin  = 4;
    }
    virtual void onDialogShow() = 0;
};

class guidialog_settings final : public guidialog_base {
    struct dialog_entry;
    std::vector<dialog_entry*> entries;
    dialog_entry* activeEntry = nullptr;
    guibutton btnSave;
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

class guidropdown_setting_options_t;
class guidropdown_setting_options_ctxt_t final : public guictxtmenu {
    guidropdown_setting_options_t* parent;
    std::vector<String> strings;

public:
    explicit guidropdown_setting_options_ctxt_t(guidropdown_setting_options_t* _parent);
    bool clickedElement(ctxtmenu_entry* e, int _id) override;
};

class guidropdown_setting_options_t final : public guidropdownbase {
public:
    std::vector<String> options;
    std::function<void(int32_t)> cbOnOptionSelected;
    std::function<String()> fnGetCurrentVal;
    std::function<int32_t()> fnGetCurrentIdx;

public:
    int32_t getSelectIndex() override { return fnGetCurrentIdx ? fnGetCurrentIdx() : -1; }
    int32_t getLastIndex() override { return CtrSize(options) - 1; }
    void setSelectedIndex(int32_t idx)  override { clicked(idx); }
    String getString() override { return fnGetCurrentVal ? fnGetCurrentVal() : "<null>"; }
    void handleDraggedRelease(MouseEvent& evt) override;
    std::vector<String>& getOptions() { return options; }
    void clicked(uint32_t idx) {
        if (cbOnOptionSelected && idx < options.size()) 
            cbOnOptionSelected(static_cast<int32_t>(idx));
    }
};

setting_dialog* makeKeybindsDialog(DawInstance* daw);

} // namespace DAW::DialogSettings
