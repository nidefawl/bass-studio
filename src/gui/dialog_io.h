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
class guidialog_iosettings : public guidialog_base {
	guidropdownbase* selectAPI;
	gui_list* deviceListInput;
	gui_list* deviceListOutput;
	guidropdownbase* audioBlockSize;
	guidropdownbase* audioSampleRate;
	guibutton btnAdd;
	guibutton btnClose;
public:
	guidialog_iosettings();
	~guidialog_iosettings();
	void render(NVGcontext* vg) override;
	void layout() override;
	void buttonClicked(guibase* button) override;
};
