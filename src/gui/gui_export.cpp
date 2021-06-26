#include <algorithm>
#include "list.h"
#include "gui.h"
#include "guicontainer.h"
#include "guicolors.h"
#include "guiconstant.h"
#include "textfield.h"
#include "renderresources.h"
#include "host/plugin/base_plugin.h"
#include "host/mainctrl.h"
#include "host/vst_host.h"
#include "platform.h"
#include "audiobuffer.h"
#include "guiscrollcontainer.h"
#include "guicontextmenu.h"
#include "button.h"
#include "host/audio_host.h"
#include "guitooltip.h"
#include "tempocontrols.h"

namespace {
constexpr int TEXT_FONT_SIZE = 20;
const SupportedFileType FILE_TYPE_EXPORT {"*.wav", "wav"};

std::vector<SupportedFileType> vFILE_TYPE_EXPORT = { FILE_TYPE_EXPORT };
}

struct export_settings_t {
	tick_t exportPos = 0;
	tick_t exportLen = 0;
	String exportPath = "";
};
class guictr_timeframe : public guictr_base {
	gui_timeinput tmTickStart;
	gui_timeinput tmTickLen;
	bool locked = false;
	guibuttonbase btnLock;
	int32_t widthText = 0;
public:
	guictr_timeframe(tick_t* s, tick_t* d) :
		guictr_base(),
		tmTickStart(s),
		tmTickLen(d, true)
	{
		padding = 0;
		margin = 0;
		tmTickStart.setLabel("Start");
		tmTickLen.setLabel("Length");

		btnLock.setLabel("Lock Start and Length");
		btnLock.drawFn = drawTextureSymbol;
		btnLock.drawParm = ICON_OPT_UNLOCKED;
		btnLock.pos = ivec2(INSET_CTR_SPACING, INSET_CTR_SPACING);
		setCanMouseHit(true);
		add(&tmTickStart);
		add(&tmTickLen);
		add(&btnLock);

	}
	~guictr_timeframe() {
		removeGuis();
	}
	void onTick(AppCtrl* ctrl) override {
		guictr_base::onTick(ctrl);
		btnLock.drawParm = locked ? ICON_OPT_LOCKED : ICON_OPT_UNLOCKED;
	}
	void layout() {

		ivec2 cs = getSizeContent();
		int32_t inset = INSET_CTR_SPACING;
		inset = 5;
		int32_t height = 20;
		int32_t widthLock = height;
		int32_t widthStartAndLen = cs.x-inset*2 - widthLock;
		int32_t widthSingle = widthStartAndLen/2;

		tmTickStart.pos = ivec2(widthSingle*1/3, 0) + ivec2(inset);
		tmTickLen.pos = ivec2(widthSingle + widthSingle*1/3, 0) + ivec2(inset);
		tmTickStart.size = ivec2(widthSingle-tmTickStart.left(), height);
		tmTickLen.size = tmTickStart.size;
		btnLock.pos = ivec2(widthStartAndLen, 0) + ivec2(inset);
		btnLock.size = ivec2(widthLock);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	virtual void render(NVGcontext* vg) {
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}

		setFont(vg, TEXT_FONT_SIZE, G_WHITE, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
		nvgText(vg, 0, this->tmTickStart.bottom(), StringAsCStr(this->tmTickStart.getLabel()), NULL);
		nvgText(vg, this->tmTickStart.right()+5, this->tmTickLen.bottom(), StringAsCStr(this->tmTickLen.getLabel()), NULL);


		for (auto* g : guis) {
			nvgSave(vg);
			g->render(vg);
			nvgRestore(vg);
		}
	}

	void buttonClicked(guibase* button) {
		if (button == &btnLock) {
			locked = !locked;
			btnLock.drawParm = locked?ICON_OPT_LOCKED : ICON_OPT_UNLOCKED;

		}
	}
	bool isLocked() const {
		return this->locked;
	}
};
class gui_export : public guictr_base {
	export_settings_t settings;
	guictr_timeframe tmFrameExport;
	guibutton btnExport;
	guibutton selectFolder;
public:
	gui_export() :
		guictr_base(),
		tmFrameExport(&settings.exportPos, &settings.exportLen)
	{
		ctrType = CTR_TYPE_EXPORT;
		getContainerLabel(ctrType, this->label);
		setBackgroundRendered(true);
		selectFolder.id = 0x10;
		selectFolder.setText(settings.exportPath);
		selectFolder.setTooltipText(settings.exportPath);
		selectFolder.setLabel("Path");
		btnExport.setLabel("Export");
		btnExport.setText(btnExport.getLabel());


		add(&tmFrameExport);
		add(&selectFolder);
		add(&btnExport);
	}
	~gui_export() {
		removeGuis();
	}
	void onTick(AppCtrl* ctrl) override {
		guictr_base::onTick(ctrl);
		if (!tmFrameExport.isLocked()) {
			auto& globals = DawInstance::get()->getGlobals();
			if (globals.cursor.getRange()) {
				settings.exportPos = globals.cursor.getTickBegin();
				settings.exportLen = globals.cursor.getRange();
			} else if (globals.loopEnabled) {
				settings.exportPos = globals.loopStart;
				settings.exportLen = globals.loopLen;
			}
		}
	}
	void layout() {
		int32_t inset = INSET_CTR_SPACING;

		ivec2 cs = getSizeContent();
		int32_t closeSize = 32;

		btnExport.size = ivec2(math::min(cs.x/2-inset*2, closeSize * 3), closeSize);
		btnExport.pos = ivec2(cs.x - btnExport.size.x + inset, cs.y-inset- btnExport.size.y);


		inset = 5;
		int32_t buttonW = math::max(120, cs.x*2/3);
		int32_t heightList = math::max(230, cs.y*2/5);
		int32_t height = 20;
		selectFolder.size = ivec2(buttonW, height);
		selectFolder.pos = ivec2(cs.x-inset*2-buttonW, inset);
		tmFrameExport.size = ivec2(cs.x, height*3);
		tmFrameExport.pos = ivec2(0, selectFolder.bottom()+inset);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	virtual void render(NVGcontext* vg) {
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}

		float lineh;
		setFont(vg, TEXT_FONT_SIZE, G_WHITE, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
		nvgTextMetrics(vg, NULL, NULL, &lineh);
		nvgText(vg, 5, this->selectFolder.bottom(), StringAsCStr(this->selectFolder.getLabel()), NULL);

		for (auto* g : guis) {
			nvgSave(vg);
			g->render(vg);
			nvgRestore(vg);
		}
	}

	void buttonClicked(guibase* button) {

		if (button->id == 0x10) {
			selectFolder.setText(settings.exportPath);
			//select folder
            String lastPath = settings.exportPath;

			replaceString(lastPath, "/", "\\");
			auto window = parentCtrl->window;

			String path = lastPath;
			if (promptUserFilePath(window, 1, vFILE_TYPE_EXPORT, path)) {
				settings.exportPath = path;
			} else {
				settings.exportPath = "";
			}
			selectFolder.setText(settings.exportPath);
			selectFolder.setTooltipText(settings.exportPath);
			return;
		}
	}
};


guictr_base* makeGuiExport() {
	return new gui_export();
}
