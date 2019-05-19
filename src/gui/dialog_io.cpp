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
#include "portaudio.h"
#include "host/vst_host.h"
#include "host/audio_host.h"
#include "host/mainctrl.h"
#include "appsettings.h"
#include "list.h"
#include "renderresources.h"
#include "platform.h"

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



class gui_device_list_entry : public gui_list_entry {
	String deviceAPI;
	String deviceName;
	const bool isInput;
public:
	gui_device_list_entry(String _deviceAPI, String _deviceName, bool _isInput) : gui_list_entry(), deviceAPI(_deviceAPI), deviceName(_deviceName), isInput(_isInput) {
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
guidialog_iosettings::~guidialog_iosettings() {
	removeGuis();
	delete deviceListInput;
	delete deviceListOutput;
	delete selectAPI;
	delete audioBlockSize;
	delete audioSampleRate;

}
guidialog_iosettings::guidialog_iosettings()
: guidialog_base(ivec2{640, 660})
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
	auto updateOptions = [this]() {
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
					_newListOut.push_back(new gui_device_list_entry{deviceAPIName, info->name, false});
				}
				if (info && info->hostApi == deviceApiIdxSelected && info->maxInputChannels > 0) {
					_newListIn.push_back(new gui_device_list_entry{deviceAPIName, info->name, true});
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
	api->cbOnOptionSelected = [api, updateOptions](int option) {
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
	add(&btnClose);
	btnClose.id = ID_BTN_CLOSE;
	btnClose.setText("Close");
	btnClose.setFontSize(BTN_FONT_SIZE);
	setLabel("Settings");
}

void guidialog_iosettings::render(NVGcontext* vg) {
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
void guidialog_iosettings::layout() {
	ivec2 cs = getSizeContent();
	int32_t closeSize = 32;
	btnClose.size = ivec2(closeSize * 4, closeSize);
	btnClose.pos = ivec2(cs.x - btnClose.size.x, cs.y - btnClose.size.y);

	cs.y -= btnClose.size.y;
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

void guidialog_iosettings::buttonClicked(guibase* button) {
	if ((button->id&0x0F) == 0xF) {
		updateSrBs();
		return;
	}
	switch (button->id) {
	case ID_BTN_CLOSE:
		closeContextMenu();
		break;

	}
}
