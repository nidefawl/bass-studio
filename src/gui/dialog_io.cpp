#include "dialog_io.h"
#include "dialog.h"
#include "math/vec.h"
#include "str_util.h"
#include "button.h"
#include "guicontainer.h"
#include "guicontextmenu_base.h"
#include "buildinfo.h"
#include "dropdown.h"
#include "guicontextmenu.h"
#include "host/vst_host.h"
#include "host/audio_host.h"
#include "host/midi_host.h"
#include "host/mainctrl.h"
#include "appsettings.h"
#include "list.h"
#include "renderresources.h"
#include "platform.h"
#include <portaudio.h>
#include <portmidi.h>

constexpr int ID_BTN_CLOSE = 1;
constexpr int TITLE_FONT_SIZE = 30;
constexpr int TEXT_FONT_SIZE = 20;
constexpr int BTN_FONT_SIZE = 16;

class guidropdown_setting_options_t;
class guidropdown_setting_options_ctxt_t: public guictxtmenu {
	guidropdown_setting_options_t* parent;
	std::vector<String> strings;
public:
	guidropdown_setting_options_ctxt_t(guidropdown_setting_options_t* _parent);
	void clicked(int _id);
};
class guidropdown_setting_options_t: public guidropdownbase {
public:
	std::vector<String> options;
	std::function<void(int)> cbOnOptionSelected;
	std::function<String()> fnGetCurrentVal;
	String value;
public:
	String getString() {
		return fnGetCurrentVal ? fnGetCurrentVal() : "<null>";
	}
	virtual void handleDraggedRelease(MouseEvent& evt) {
		if (options.empty())
			return;
		guictxtmenu_base *popup = new guidropdown_setting_options_ctxt_t(this);
		popup->size = size;
		popup->setFontSize(size.y);
		this->parentCtrl->openContextMenu(popup, toScreenSpace(ivec2(0, size.y))-popup->pos+ivec2(1));
	}
	std::vector<String>& getOptions() {
		return options;
	}
	void clicked(int idx) {
		if (cbOnOptionSelected) {
			if (idx >= 0 && idx < options.size()) {
				value = idx;
			}
			cbOnOptionSelected(idx);

		}

	}
};


guidropdown_setting_options_ctxt_t::guidropdown_setting_options_ctxt_t(guidropdown_setting_options_t* _parent) : parent(_parent) {
	this->size.x = 120;
	this->fontSize = FONT_SIZE_CTXT_SMALL;
	this->paddingV = 0;
	std::vector<String>& options = parent->getOptions();

	int32_t idx = 0;
	for (String option : options) {
		addEntry(new ctxtmenu_entry(option, idx++));
	}

}
void guidropdown_setting_options_ctxt_t::clicked(int _id) {
	closeContextMenu();
	parent->clicked(_id);
}



class gui_listentry_audiodevice : public gui_list_entry {
	String deviceAPI;
	String deviceName;
	const bool isInput;
public:
	gui_listentry_audiodevice(String _deviceAPI, String _deviceName, bool _isInput) : gui_list_entry(), deviceAPI(_deviceAPI), deviceName(_deviceName), isInput(_isInput) {
		icon = -1;
	}
	String getText() override {
		return deviceName;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
	}
	void handleDraggedBegin(MouseEvent& evt) override {
		toggle();
	}
	std::vector<app_io>& getCnf() {
		return isInput ? settings.iosettings.getConfig(deviceAPI).inputs : settings.iosettings.getConfig(deviceAPI).outputs;
	}
	bool enabled() {
		auto& c = getCnf();
		auto it = std::find_if(c.begin(), c.end(), [devN=deviceName](const app_io& config){
			return config.deviceName == devN;
		});
		if (it != c.end()) {
			return true;
		}
		return false;
	}
	bool toggle() {
		bool bEnbl = enabled();
		auto& c = getCnf();
		erase_if(c, [devN=deviceName](const app_io& config) {
			return config.deviceName == devN;
		});
		if (!bEnbl) {
			c.push_back({0, deviceName});
		}
		if (parent && parent->parent) {
			parent->parent->buttonClicked(this);
		}
		return false;
	}
	void render(NVGcontext* vg) override {
		BaseCtrl* ctrl = parentCtrl;
		float spacing = INSET_TITLE;
		float x = spacing;
		float rowHeight = size.y;
		if (icon > -1) {
			x += rowHeight + spacing;
		}
		if (ctrl->isCtrOrChildFocused(this)) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER));
			nvgFill(vg);
		}
		nvgTranslate(vg, pos.x, pos.y);
		if (icon > -1) {
			RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
			drawIcon(vg, size, &image);
		}
		setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
//		auto* _entry = safeRefGet(ref);
//		if (_entry) {
		bool enbl = enabled();
			setFont(vg, (int) (rowHeight * 0.8), theme->getColor(enbl?GuiColor::COL_ON:GuiColor::COL_OFF), G_TITLE_ALIGN);
			nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
			String str = enbl?"On":"Off";
			nvgText(vg, size.x-spacing, rowHeight / 2, StringAsCStr(str), NULL);
//		}
//		nvgBeginPath(vg);
//		int i2 = 4;
//		nvgRect(vg, i2, i2, size.x-i2*2, size.y-i2*2);
//		nvgFillColor(vg, rgbToNvg(0xFF11ff11));
//		nvgFill(vg);
		nvgTranslate(vg, -pos.x, -pos.y);
	}
};
void updateSrBs() {
	auto mctrl = MainCtrl::get();
	bool b = mctrl->isPlaying();
	if (b) {

		mctrl->stopPlaying();
		mctrl->cursor.cursorPos = mctrl->getPlaybackPos();
	}
	{

		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		vsthost* host = vsthost::getInstance();
		audiohost* ahost = audiohost::getInstance();
		ahost->stopAudio();
		host->setOutput(nullptr);
		if (ahost->startAudio()) {
			host->setOutput(ahost);
		}
	}
	if (b) {
		mctrl->startPlaying();
	}
};

class guidialog_audio_io : public setting_dialog {
	guidropdownbase* selectAPI;
	gui_list* deviceListInput;
	gui_list* deviceListOutput;
	guidropdownbase* audioBlockSize;
	guidropdownbase* audioSampleRate;
public:
	void onDialogShow() override {
		updateOptions();
	}
	void updateOptions() {

		if (audiohost::getInstance()->initPa()) {
			String deviceAPIName = settings.iosettings.device_api;
			int apiCount = Pa_GetHostApiCount();
			int deviceApiIdxSelected = -1;
			for (int i = 0; i < apiCount; i++) {
				const PaHostApiInfo *info = Pa_GetHostApiInfo(i);
				if (info) {
					if (deviceApiIdxSelected < 0 || !strcmp(StringAsCStr(settings.iosettings.device_api), info->name)) {
						deviceApiIdxSelected = i;
						deviceAPIName = info->name;
					}
				}
			}
			std::vector<gui_list_entry*> _newListIn;
			std::vector<gui_list_entry*> _newListOut;
			int deviceCount = Pa_GetDeviceCount();
			for (int i = 0; i < deviceCount; i++) {
				const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
				if (info && info->hostApi == deviceApiIdxSelected && info->maxOutputChannels > 0) {
					_newListOut.push_back(new gui_listentry_audiodevice{deviceAPIName, info->name, false});
				}
				if (info && info->hostApi == deviceApiIdxSelected && info->maxInputChannels > 0) {
					_newListIn.push_back(new gui_listentry_audiodevice{deviceAPIName, info->name, true});
				}
			}
			int idx = 0;
			for (auto* p : _newListIn) {
				p->id = 0x1f|(idx++<<8);
			}
			idx = 0;
			for (auto* p : _newListOut) {
				p->id = 0x0f|(idx++<<8);
			}
			deviceListInput->setList(_newListIn);
			deviceListOutput->setList(_newListOut);
			deviceListInput->layout();
			deviceListOutput->layout();
		}

	}
	~guidialog_audio_io() {
		removeGuis();
		delete deviceListInput;
		delete deviceListOutput;
		delete selectAPI;
		delete audioBlockSize;
		delete audioSampleRate;

	}
	guidialog_audio_io()
	: setting_dialog()
	{
		deviceListInput = new gui_list();
		deviceListOutput = new gui_list();
		auto audioBlockSize = new guidropdown_setting_options_t{};
		this->audioBlockSize = audioBlockSize;
		auto audioSampleRate = new guidropdown_setting_options_t{};
		this->audioSampleRate = audioSampleRate;
		int srates[] = {
				44100, 48000, 96000, 192000
		};
		for (int i = 0; i < 4; i++) {
			audioSampleRate->options.push_back(StringFormat("%d", srates[i]));
		}
		audioSampleRate->cbOnOptionSelected = [srates](int option) {
			if (option >= 0 && option < 4) {
				settings.iosettings.samplerate = srates[option];
				updateSrBs();
			}
		};
		audioSampleRate->fnGetCurrentVal = []() -> String {
			return StringFormat("%d", settings.iosettings.samplerate);
		};
		for (int i = 0; i < 10; i++) {
			int blockSize = 1<<(4+i);
			audioBlockSize->options.push_back(StringFormat("%d", blockSize));
		}
		audioBlockSize->cbOnOptionSelected = [](int option) {
			if (option >= 0 && option < 10) {
				int blockSize = 1<<(4+option);
				settings.iosettings.blocksize = blockSize;
				updateSrBs();
			}
		};
		audioBlockSize->fnGetCurrentVal = []() -> String {
			return StringFormat("%d", settings.iosettings.blocksize);
		};
		guidropdown_setting_options_t* api = new guidropdown_setting_options_t{};
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		if (audiohost::getInstance()->initPa()) {
			int apiCount = Pa_GetHostApiCount();
			for (int i = 0; i < apiCount; i++) {
				const PaHostApiInfo *info = Pa_GetHostApiInfo(i);
				if (info) {
					if (settings.iosettings.device_api.empty()) {
						settings.iosettings.device_api = info->name;
					}
					api->options.push_back(String{info->name});
				}
			}
		}
		updateOptions();
		api->cbOnOptionSelected = [this, api](int option) {
			if (option >= 0 && option < api->options.size()) {
				ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
				vsthost* host = vsthost::getInstance();
				audiohost* ahost = audiohost::getInstance();
				ahost->stopAudio();
				host->setOutput(nullptr);
				settings.iosettings.device_api = api->options[option];
				updateOptions();
				if (ahost->startAudio()) {
					host->setOutput(ahost);
				}
			}
		};
		api->fnGetCurrentVal = []() -> String {
			return settings.iosettings.device_api;
		};
		selectAPI = api;

		setBackgroundRendered(true);
		add(selectAPI);
		add(deviceListInput);
		add(deviceListOutput);
		add(audioBlockSize);
		add(audioSampleRate);
		selectAPI->setFontScale(0.77f);
	//	selectDevice->setFontScale(0.77f);
		selectAPI->setLabel("Audio API");
		audioBlockSize->setLabel("Blocksize");
		audioSampleRate->setLabel("Samplerate");
		deviceListInput->setLabel("Audio input device");
		deviceListOutput->setLabel("Audio output device");
		setLabel("Audio I/O");
	}

	void render(NVGcontext* vg) {
		if (isBackgroundRendered()){
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}

		float lineh;
		setFont(vg, TEXT_FONT_SIZE, G_WHITE, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
		nvgTextMetrics(vg, NULL, NULL, &lineh);
		nvgText(vg, 5, this->audioBlockSize->bottom(), StringAsCStr(this->audioBlockSize->label), NULL);
		nvgText(vg, 5, this->audioSampleRate->bottom(), StringAsCStr(this->audioSampleRate->label), NULL);
		nvgText(vg, 5, this->selectAPI->bottom(), StringAsCStr(this->selectAPI->label), NULL);
		nvgText(vg, 5, this->deviceListInput->top()-2, StringAsCStr(this->deviceListInput->label), NULL);
		nvgText(vg, 5, this->deviceListOutput->top()-2, StringAsCStr(this->deviceListOutput->label), NULL);

		for (auto c : guis) {
			nvgSave(vg);
	//		if (c == this->selectAPI) {
	//			nvgIntersectScissor(vg, c->pos.x, c->pos.y, c->size.x, c->size.y);
	//		}
			c->render(vg);
			nvgRestore(vg);
		}
	}
	void layout() {
		ivec2 cs = getSizeContent();

		int32_t inset = 5;
		int32_t buttonW = math::max(120, cs.x*2/3);
		int32_t heightList = math::max(230, cs.y*2/5);
		int32_t height = 20;
		audioBlockSize->size = ivec2(buttonW, height);
		audioBlockSize->pos = ivec2(cs.x-inset*2-buttonW, inset);
		audioSampleRate->size = ivec2(buttonW, height);
		audioSampleRate->pos = ivec2(cs.x-inset*2-buttonW, audioBlockSize->bottom()+inset);
		selectAPI->size = ivec2(buttonW, height);
		selectAPI->pos = ivec2(cs.x-inset*2-buttonW, audioSampleRate->bottom()+inset);
		deviceListInput->pos = ivec2(inset, selectAPI->bottom()+inset+(int32_t)(TEXT_FONT_SIZE*1.2));
		deviceListInput->size = ivec2((cs.x)-inset*2, heightList);
		deviceListOutput->pos = ivec2(inset, deviceListInput->bottom()+inset+(int32_t)(TEXT_FONT_SIZE*1.2));
		deviceListOutput->size = ivec2((cs.x)-inset*2, math::min(cs.y-deviceListOutput->pos.y, heightList));
		for (auto gui : guis) {
			gui->layout();
		}
	}
	void buttonClicked(guibase* button) {
		if ((button->id&0x0F) == 0xF) {
			updateSrBs();
			return;
		}
		if (this->parent) {
			this->parent->buttonClicked(button);
		}
	}


};
class gui_listentry_mididevice : public gui_list_entry {
	String deviceAPI;
	String deviceName;
	const bool isInput;
public:
	gui_listentry_mididevice(String _deviceAPI, String _deviceName, bool _isInput) : gui_list_entry(), deviceAPI(_deviceAPI), deviceName(_deviceName), isInput(_isInput) {
		icon = -1;
	}
	String getText() override {
		return deviceName;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
	}
	void handleDraggedBegin(MouseEvent& evt) override {
		toggle();
	}
	std::vector<app_io>& getCnf() {
		return isInput ? settings.iosettings.getIOConfigMidi(deviceAPI).inputs : settings.iosettings.getIOConfigMidi(deviceAPI).outputs;
	}
	bool enabled() {
		auto& c = getCnf();
		auto it = std::find_if(c.begin(), c.end(), [devN=deviceName](const app_io& config){
			return config.deviceName == devN;
		});
		if (it != c.end()) {
			return true;
		}
		return false;
	}
	bool toggle() {
		bool bEnbl = enabled();
		auto& c = getCnf();
		erase_if(c, [devN=deviceName](const app_io& config) {
			return config.deviceName == devN;
		});
		if (!bEnbl) {
			c.push_back({0, deviceName});
		}
		if (parent && parent->parent) {
			parent->parent->buttonClicked(this);
		}
		return false;
	}
	void render(NVGcontext* vg) override {
		BaseCtrl* ctrl = parentCtrl;
		float spacing = INSET_TITLE;
		float x = spacing;
		float rowHeight = size.y;
		if (icon > -1) {
			x += rowHeight + spacing;
		}
		if (ctrl->isCtrOrChildFocused(this)) {
			nvgBeginPath(vg);
			nvgRect(vg, pos.x, pos.y, size.x, size.y);
			nvgFillColor(vg, theme->getColor(GuiColor::COL_BG_DRKER));
			nvgFill(vg);
		}
		nvgTranslate(vg, pos.x, pos.y);
		if (icon > -1) {
			RenderResources::NvgImageTexture& image = RenderResources::imgIcons[icon];
			drawIcon(vg, size, &image);
		}
		setFont(vg, (int) (rowHeight * 0.8), G_WHITE, G_TITLE_ALIGN);
		nvgText(vg, x, rowHeight / 2, StringAsCStr(getText()), NULL);
//		auto* _entry = safeRefGet(ref);
//		if (_entry) {
		bool enbl = enabled();
			setFont(vg, (int) (rowHeight * 0.8), theme->getColor(enbl?GuiColor::COL_ON:GuiColor::COL_OFF), G_TITLE_ALIGN);
			nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
			String str = enbl?"On":"Off";
			nvgText(vg, size.x-spacing, rowHeight / 2, StringAsCStr(str), NULL);
//		}
//		nvgBeginPath(vg);
//		int i2 = 4;
//		nvgRect(vg, i2, i2, size.x-i2*2, size.y-i2*2);
//		nvgFillColor(vg, rgbToNvg(0xFF11ff11));
//		nvgFill(vg);
		nvgTranslate(vg, -pos.x, -pos.y);
	}
};


class guidialog_midi_io : public setting_dialog {
	gui_list* deviceListInput;
	gui_list* deviceListOutput;
public:
	void onDialogShow() override {
		updateOptions();
	}

	~guidialog_midi_io() {
		removeGuis();
		delete deviceListInput;
		delete deviceListOutput;
	}
	guidialog_midi_io()
	: setting_dialog()
	{
		deviceListInput = new gui_list();
		deviceListOutput = new gui_list();

		setBackgroundRendered(true);
		add(deviceListInput);
		add(deviceListOutput);
		deviceListInput->setLabel("Midi input device");
		deviceListOutput->setLabel("Midi output device");
		setLabel("Midi I/O");
		updateOptions();
	}

	void render(NVGcontext* vg) {
		if (isBackgroundRendered()){
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}

		float lineh;
		setFont(vg, TEXT_FONT_SIZE, G_WHITE, NVG_ALIGN_BOTTOM | NVG_ALIGN_LEFT);
		nvgTextMetrics(vg, NULL, NULL, &lineh);
		nvgText(vg, 5, this->deviceListInput->top()-2, StringAsCStr(this->deviceListInput->label), NULL);
		nvgText(vg, 5, this->deviceListOutput->top()-2, StringAsCStr(this->deviceListOutput->label), NULL);

		for (auto c : guis) {
			nvgSave(vg);
	//		if (c == this->selectAPI) {
	//			nvgIntersectScissor(vg, c->pos.x, c->pos.y, c->size.x, c->size.y);
	//		}
			c->render(vg);
			nvgRestore(vg);
		}
	}
	void layout() {
		ivec2 cs = getSizeContent();

		int32_t inset = 5;
		int32_t buttonW = math::max(120, cs.x*2/3);
		int32_t heightList = math::max(230, cs.y*2/5);
		deviceListInput->pos = ivec2(inset, inset+(int32_t)(TEXT_FONT_SIZE*1.2));
		deviceListInput->size = ivec2((cs.x)-inset*2, heightList);
		deviceListOutput->pos = ivec2(inset, deviceListInput->bottom()+inset+(int32_t)(TEXT_FONT_SIZE*1.2));
		deviceListOutput->size = ivec2((cs.x)-inset*2, math::min(cs.y-deviceListOutput->pos.y, heightList));
		for (auto gui : guis) {
			gui->layout();
		}
	}
	void buttonClicked(guibase* button) {
		if ((button->id&0x0F) == 0xF) {
		    midihost::getInstance()->reopenAllConfiguredDevices(false);
//			updateSrBs();
			return;
		}
		if (this->parent) {
			this->parent->buttonClicked(button);
		}
	}


	void updateOptions() {
		if (midihost::getInstance()->initPm()) {
		    for (int i = 0; i < Pm_CountDevices(); i++) {
		        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
		        if (info->input) log_printf("%d: %s, %s\n", i, info->interf, info->name);
		    }
		    printf("MIDI output devices:\n");
		    for (int i = 0; i < Pm_CountDevices(); i++) {
		        const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
		        if (info->output) log_printf("%d: %s, %s\n", i, info->interf, info->name);
		    }


			std::vector<gui_list_entry*> _newListIn;
			std::vector<gui_list_entry*> _newListOut;
			int deviceCount = Pm_CountDevices();
			for (int i = 0; i < deviceCount; i++) {
				const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
				if (info && info->output > 0) {
					_newListOut.push_back(new gui_listentry_mididevice{"stdmidi", info->name, false});
				}
				if (info && info->input > 0) {
					_newListIn.push_back(new gui_listentry_mididevice{"stdmidi", info->name, true});
				}
			}
			int idx = 0;
			for (auto* p : _newListIn) {
				p->id = 0x1f|(idx++<<8);
			}
			idx = 0;
			for (auto* p : _newListOut) {
				p->id = 0x0f|(idx++<<8);
			}
			deviceListInput->setList(_newListIn);
			deviceListOutput->setList(_newListOut);
			deviceListInput->layout();
			deviceListOutput->layout();
		}
	};
};

struct guidialog_settings::dialog_entry
{
	guibutton tabButton;
	setting_dialog* tabCtr;
	bool active = false;
	dialog_entry(setting_dialog* _ctr, String title) : tabButton(), tabCtr(_ctr) {
		tabButton.setText(title);
		tabButton.setEnabledRef(&active);
		tabButton.setFontScale(0.7f);
	}
};
guidialog_settings::guidialog_settings()
: guidialog_base(ivec2{640, 660}) {
	addEntry(new guidialog_audio_io(), "Audio I/O");
	addEntry(new guidialog_midi_io(), "Midi I/O");
	add(&btnClose);
	btnClose.id = ID_BTN_CLOSE;
	btnClose.setText("Close");
	btnClose.setFontSize(BTN_FONT_SIZE);
	setLabel("Settings");
	setActiveEntry(0);
};
void guidialog_settings::addEntry(setting_dialog* ctr, String title) {
	guidialog_settings::dialog_entry* entry = new guidialog_settings::dialog_entry{ctr, title};
	guictr_base::add(&entry->tabButton);
	this->entries.push_back(entry);
}
void guidialog_settings::setActiveEntry(int32_t idx) {
	if (idx >= 0 && idx < entries.size()) {
		guidialog_settings::dialog_entry* entry = entries[idx];
		if (this->activeEntry) {
			this->activeEntry->active = false;
			this->removeUNCHECKED(this->activeEntry->tabCtr);
		}
		this->activeEntry = entry;
		this->activeEntry->active = true;
		this->add(this->activeEntry->tabCtr);
		if (this->parentCtrl) {
			this->layout();
		}
		this->activeEntry->tabCtr->onDialogShow();
	}

}
guidialog_settings::~guidialog_settings() {
	for (auto* entry : entries) {
		remove(&entry->tabButton);
	}
	remove(&btnClose);
	// only this->activeEntry->tabCtr should be in this cointainer
	// at this point. And it must be a valid pointer
	dbgassert(guis.size() <= 1);
	removeGuis();
	for (auto* entry : entries) {
		delete entry->tabCtr;
		delete entry;
	}
};

void guidialog_settings::render(NVGcontext* vg) {
	guictr_base::render(vg);
}
void guidialog_settings::layout() {
	int32_t inset = INSET_CTR_SPACING;

	ivec2 csize = getSizeContent();
	int32_t closeSize = 32;
	btnClose.size = ivec2(closeSize * 4, closeSize);
	btnClose.pos = ivec2(csize.x - btnClose.size.x-inset, csize.y - btnClose.size.y);

	csize.y -= btnClose.size.y;

	int csW = csize.x - inset * 2;
	ivec2 buttonPos = { inset, inset*2 };
	int32_t buttonW = csW / 5;
	for (auto* entry : entries) {
		entry->tabButton.pos = buttonPos;
		entry->tabButton.size = ivec2(buttonW, HEIGHT_DEFAULT_INPUT);
		entry->tabButton.layout();
		buttonPos.y += HEIGHT_DEFAULT_INPUT + inset;
	}
	ivec2 sizeContentTab = ivec2(csW-buttonW-inset*2, csize.y-inset*2);
	for (auto* entry : entries) {
		entry->tabCtr->pos = ivec2(buttonPos.x+buttonW+inset*2, inset);
		entry->tabCtr->size = sizeContentTab;
		entry->tabCtr->determineSize(entry->tabCtr->size);
		entry->tabCtr->layout();
	}

	for(auto gui : guis) {
		gui->layout();
	}
}
void guidialog_settings::buttonClicked(guibase* button) {

	auto it = std::find_if(entries.begin(), entries.end(), [button](const guidialog_settings::dialog_entry* entry) {
		return &entry->tabButton == button;
	});
	if (it != entries.end()) {
		size_t pos = it-entries.begin();
		setActiveEntry((int32_t) pos);
	}
//	if (parent) {
//		parent->buttonClicked(button);
//	}
	switch (button->id) {
	case ID_BTN_CLOSE:
		closeContextMenu();
		break;

	}
}
