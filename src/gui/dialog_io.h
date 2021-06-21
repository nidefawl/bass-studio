#pragma once
#include "math/vec.h"
#include "str_util.h"
#include "knob.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "button.h"
#include <vector>
#include "dialog.h"

class gui_list;
class guidropdownbase;
class setting_dialog : public guictr_base {
public:
	virtual void onDialogShow() = 0;
};

class guidialog_settings : public guidialog_base {
	struct dialog_entry;
	std::vector<dialog_entry*> entries;
	dialog_entry* activeEntry = nullptr;
	guibutton btnClose;
	void init();
public:
	guidialog_settings();
	guidialog_settings(ivec2 _dialogSize, bool _resizeable = false);
	~guidialog_settings();
	void render(NVGcontext* vg) override;
	void layout() override;
	void buttonClicked(guibase* button) override;
	int32_t getNumEntries();
	void setActiveEntry(int32_t idx);
	void addEntry(setting_dialog* ctr, String title);
};
