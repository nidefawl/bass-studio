#include <nanovg.h>
#include <time.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>
#include <GLFW/glfw3.h>

#include "window.h"
#include "platform.h"

#include "keyboard.h"
#include "commands.h"

#include "basectrl.h"
#include "exceptions.h"
#include "color_util.h"
#include "str_util.h"
#include "logging.h"
#include "menu.h"
#include "msgbox.h"
#include "tls.h"

#include "gui/gui.h"
#include "gui/guicontainer.h"
#include "gui/knob.h"
#include "gui/button.h"
#include "gui/guicolorpick.h"
#include "gui/guiinputfield.h"
#include "gui/splitter.h"
#include "gui/guicontextmenu.h"
#include "gui/scrollbar.h"
#include "gui/statusbar.h"
#include "gui/guimenu.h"
#include "audiocache.h"
#include "gui/drawwaveform.h"
#include "gui/guicontainer_layout.h"
#include "fileio.h"
#include "logging.h"
#include <soxr.h>

int startApplication(int argc, char* argv[]);


struct Menus {
	ngui::Menu file;
};

guictr_base* makeCtrTheme();
guictr_base* makeCtrProperties();
namespace MiniApp {
	class guictr_tabbed_test : public guictr_tabbed {
	public:
		guictr_base* const ctr_properties;
		guictr_base* const ctr_theme;
		guictr_tabbed_test() : guictr_tabbed(), ctr_properties(makeCtrProperties()), ctr_theme(makeCtrTheme()) {
			ctr_properties->setLabel("Properties");
			ctr_theme->setLabel("Theme");
			addEntry(ctr_theme, ctr_theme->label);
			addEntry(ctr_properties, ctr_properties->label);
			setActiveEntry(0);
		}
		virtual ~guictr_tabbed_test() {
			//remove the entries we have to delete, base class would see dangling ptr otherwise
			remove(ctr_properties);
			remove(ctr_theme);
			delete ctr_properties;
			delete ctr_theme;
		}
	};
	class gui_ctr_test : public guictr_base {

		gui_color_pick colorPick;
	//	gui_timeinput clipTimeStart;
		gui_numberinput_field field;
		gui_textfield textField;
		guictr_base* ctrTabbed;
		int nr;
	public:
		gui_ctr_test() : guictr_base(), field(nullptr), ctrTabbed(new guictr_tabbed_test()) {
			setBackgroundRendered(true);
			add(&this->colorPick);
			add(&textField);
			add(&field);
			add(ctrTabbed);
			field.setRef(&this->nr);


			textField.setCallback([](const String& str) {
				my_printf("text callback %s\n", StringAsCStr(str));
				return true;
			});
		//	textField.setPlaceholder("Search");
		}
		~gui_ctr_test() {
			removeGuis();
			delete ctrTabbed;
		}
		std::vector<String> g_debugStrings;
		void buttonClicked(guibase* button) {
			if (&field == button) {
				my_printf("New val %d\n", this->nr);
			}

		}
		void render(NVGcontext* vg) {
			if (isBackgroundRendered()){
				renderBackground(vg);
			}
			if (!setScissorTransform(vg)) {
				return;
			}
			BaseCtrl *ctrl = this->parentCtrl;

			std::vector<String> strings;
			String str;
			str = ctrl->guiOver ? ctrl->guiOver->getClassName() : "<null>";
			strings.push_back(String("guiOver: ") + str);
			str = ctrl->guiDragged ? ctrl->guiDragged->getClassName() : "<null>";
			strings.push_back(String("guiDragged: ") + str);
			str = ctrl->guiCaptured ? ctrl->guiCaptured->getClassName() : "<null>";
			strings.push_back(String("guiCaptured: ") + str);
			str = ctrl->guiCtrFocused ? ctrl->guiCtrFocused->getClassName() : "<null>";
			strings.push_back(String("guiCtrFocused: ") + str);
			str = ctrl->guiFocused ? ctrl->guiFocused->getClassName() : "<null>";
			strings.push_back(String("guiFocused: ") + str);

			guibase* p = ctrl->guiFocused;
			int lvl = 0;
			while (p != NULL) {
				String s = "";
				if (lvl == 0) {
					s = "guiFocused: ";
				}
				for (int i = 0; i < lvl; i++) {
					s += "  ";
				}
				strings.push_back(s + p->getClassName());
				p = p->parent;
				lvl++;
			}

			int y1 = 160;
			int x = 5;

			setFont(vg, 26, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		//	String proj = StringFormat("Project: %s", StringAsCStr(ctrl->getProjectPath()));
		//	nvgText(vg, x, 0, StringAsCStr(proj), NULL);
			float lineh;
			nvgTextMetrics(vg, NULL, NULL, &lineh);
			int y = y1 + lineh;
			for (String& s : strings) {
				nvgText(vg, x, y, StringAsCStr(s), NULL);
				y += lineh;
			}
			for (String& s : g_debugStrings) {
				nvgText(vg, x, y, StringAsCStr(s), NULL);
				y += lineh;
			}
			for (auto* gui : guis) {
				nvgSave(vg);
				gui->render(vg);
				nvgRestore(vg);
			}

			g_debugStrings.clear();
		}
		void layout() {
			ivec2 cs = getSizeContent();
			int q = 240;
			colorPick.size = ivec2(q*2, q);
			colorPick.pos = ivec2(cs.x/2-colorPick.size.x, cs.y-colorPick.size.y);
			field.size = ivec2(320, 32);
			field.pos = ivec2(0, 0);
			textField.size = ivec2(320, 32);
			textField.pos = ivec2(55, field.bottom()+120);
			ctrTabbed->pos = { cs.x / 2, 0 };
			ctrTabbed->size.x = cs.x/2;
			ctrTabbed->size.y = cs.y;
			ctrTabbed->layout();
			colorPick.layout();
			field.layout();
			textField.layout();
		}
		void addStr(String str) {
			g_debugStrings.push_back(std::move(str));
		}
	};

	class ViewContainersGuiTest {
	public:
		guictr_menubar ctr_menu;
		gui_statusbar statusbar;
		gui_ctr_test ctr_dbg;
		Splitter splitterCenter;
		ViewContainersGuiTest(/*const*/ AppCtrl* const ctrl) :
		  ctr_menu(ctrl->getMenubar()),
		  splitterCenter(0, 0.9f)
		{
			splitterCenter.setMinMax(0.05f, 0.95f);
		}
		void layout(int32_t winW, int32_t winH) {
			int winX = 0; int winY = 0;
			int winBottom = winH;
	#if USE_GUI_MENU
			int hMenu = 28;
			winH -= hMenu;
			winY += hMenu;
			ctr_menu.pos = vec2(0, 0);
			ctr_menu.size = vec2(winW, hMenu);
	#endif
			int hDbg = splitterCenter.leftOrTop(winH);
			int hstatusBar = splitterCenter.rightOrBottom(winH);
			statusbar.size = { winW, hstatusBar };
			statusbar.pos = { winX, winBottom - hstatusBar };
			splitterCenter.pos = ivec2(winX, statusbar.pos.y - 5);
			splitterCenter.size = ivec2(winW, 10);

	//		statusbar.setSnapSides(ivec4(0, 1, 0, 0));
			ctr_dbg.setSnapSides(ivec4(0, 0, 0, 1));


			ctr_dbg.pos = {winX, hMenu};
			ctr_dbg.size = {winW, hDbg};
		}
		void addTo(std::vector<guictr_base*>& v) {
			 v.push_back(&splitterCenter);
			 v.push_back(&statusbar);
			 v.push_back(&ctr_dbg);
#if USE_GUI_MENU
			 v.push_back(&ctr_menu);
#endif
		}
#if USE_GUI_MENU
		guictr_base* getMenuCtr() {
			return nullptr;
		}
#endif
	};
	struct oversample_config_t {
		int32_t inputSampleRate = 0;
		int32_t outputSampleRate = 0;
		int32_t numChannels = 0;
		int32_t numSamplesInput = 0;
		int32_t numSamplesResampled = 0;
		void setInputLength(int32_t numSamples) {
			numSamplesInput = numSamples;
			dbgassert(FitsTypeRange<int64_t>((int64_t)numSamplesInput * (int64_t)outputSampleRate / (double)inputSampleRate + .5));
			numSamplesResampled = (int32_t) ((int64_t)numSamplesInput * (int64_t)outputSampleRate / (double)inputSampleRate + .5);
		}
	};
	struct oversampler_t : public oversample_config_t {
		std::vector<std::vector<float>> dataIn;
		std::vector<std::vector<float>> dataOut;
		std::vector<float*> channelPtrsOut;
		std::vector<float*> channelPtrsIn;
		soxr_t soxr = 0;
		soxr_error_t soxrError = 0;
		oversampler_t(oversample_config_t cfg) {
			*static_cast<oversample_config_t*>(this) = cfg;
			dataIn.resize(numChannels);
			dataOut.resize(numChannels);
			channelPtrsIn.resize(numChannels);
			channelPtrsOut.resize(numChannels);
			float* dataInput = new float[numSamplesInput];
			float* dataOutput = new float[numSamplesResampled];
			for (int i = 0; i < numChannels; i++) {
				dataIn[i].clear();
				dataIn[i].insert(dataIn[i].begin(), dataInput, dataInput+numSamplesInput);
				dataOut[i].clear();
				dataOut[i].insert(dataOut[i].begin(), dataOutput, dataOutput+numSamplesResampled);
			}
			for (int i = 0; i < numChannels; i++) {
				channelPtrsIn[i] = dataIn[i].data();
				channelPtrsOut[i] = dataOut[i].data();
			}
			delete [] dataInput;
			delete [] dataOutput;

			soxr_quality_spec_t q_spec = soxr_quality_spec(0, 0);
			soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_S, SOXR_FLOAT32_S);
			soxr_runtime_spec_t const runtime_spec = soxr_runtime_spec(0);

//			my_printf("soxr_oneshot from %d to %d, samples %d -> %d, channels %d\n", wav.sampleRate, this->samplerate, wav.totalSampleCount, olen, wav.channels);
//			my_printf("pSamples.size %d\n", pSamples.size());
//			my_printf("pSamples2.size %d\n", pSamples2.size());

			soxr = soxr_create((double)inputSampleRate, (double)outputSampleRate, numChannels, &soxrError, &io_spec, &q_spec, &runtime_spec);


		}
		void runResample() {

			if (!!soxrError) {
				my_printf("soxr_create failed: %d %s\n", soxrError, soxr_strerror(soxrError));
			} else {
				size_t offset = 0;
				soxrError = soxr_process(soxr, channelPtrsIn.data(), numSamplesInput, NULL, channelPtrsOut.data(), numSamplesResampled, &offset);
//				my_printf("offset %d, numSamplesInput: %d\n", offset, numSamplesInput);


				if (!!soxrError) {
					my_printf("soxr_process failed: %d %s\n", soxrError, soxr_strerror(soxrError));
				} else {
//					my_printf("soxr_process success %d\n", error);
				}
			}
		}
		~oversampler_t() {
//			my_printf("%-26s\n", soxr_strerror(error));
			soxr_delete(soxr);
		}
	};
	class guictr_test_resample : public guictr_base {
		oversample_config_t oversampleCfg;
		std::shared_ptr<oversampler_t> oversampler;
		int32_t blockSize = 4;
	public:
		guictr_test_resample() : guictr_base()  {
			initResample(blockSize, 44100, 192000);
		}

		~guictr_test_resample() {
		}
		void initResample(int32_t numSamplesInput, int32_t inputSampleRate, int32_t outputSampleRate) {
			oversampleCfg.inputSampleRate = inputSampleRate;
			oversampleCfg.outputSampleRate = outputSampleRate;
			oversampleCfg.numChannels = 2;
			oversampleCfg.setInputLength(numSamplesInput);
			oversampler = std::make_shared<oversampler_t>(oversampleCfg);

		}
		int32_t loopLen = 1024;
		int32_t runs = 0;
		int64_t timeResample_usec = 0;
		void onTick(AppCtrl* ctrl) override {
			if (oversampler) {
				auto timeStart = getTimeHPint64();
				auto timeStartD = getTimeMillisd();
				for (int i = 0; i < loopLen; i++) {
					oversampler->runResample();
				}
				auto timeEnd = getTimeHPint64();
				auto timeEndD = getTimeMillisd();
				auto timeHpDuration = timeEnd - timeStart;
				auto timeMillisDuration = timeEndD - timeStartD;
				timeResample_usec = timeHpDuration;
				runs++;
				if ((runs&0x7F) == 0) {
					initResample(blockSize, 44100, 192000);
					blockSize*=2;
					if (blockSize > 1024*32) {
						loopLen = 512;
						blockSize = 4;
					}
					if (loopLen > 1)
						loopLen /= 2;
				}
			}
		}

		void render(NVGcontext* vg) override {
			if (isBackgroundRendered()){
				renderBackground(vg);
			}
			if (!setScissorTransform(vg)) {
				return;
			}
			BaseCtrl *ctrl = this->parentCtrl;

			std::vector<String> strings;
			String str;
			str = ctrl->guiOver ? ctrl->guiOver->getClassName() : "<null>";
			strings.push_back(String("guiOver: ") + str);
			str = ctrl->guiDragged ? ctrl->guiDragged->getClassName() : "<null>";
			strings.push_back(String("guiDragged: ") + str);
			str = ctrl->guiCaptured ? ctrl->guiCaptured->getClassName() : "<null>";
			strings.push_back(String("guiCaptured: ") + str);
			str = ctrl->guiCtrFocused ? ctrl->guiCtrFocused->getClassName() : "<null>";
			strings.push_back(String("guiCtrFocused: ") + str);
			str = ctrl->guiFocused ? ctrl->guiFocused->getClassName() : "<null>";
			strings.push_back(String("guiFocused: ") + str);
			strings.push_back(StringFormat("oversampleCfg.inputSampleRate: %d", oversampleCfg.inputSampleRate));
			strings.push_back(StringFormat("oversampleCfg.outputSampleRate: %d", oversampleCfg.outputSampleRate));
			strings.push_back(StringFormat("oversampleCfg.numChannels: %d", oversampleCfg.numChannels));
			strings.push_back(StringFormat("oversampleCfg.numSamplesInput: %d", oversampleCfg.numSamplesInput));
			strings.push_back(StringFormat("oversampleCfg.numSamplesResampled: %d", oversampleCfg.numSamplesResampled));
			strings.push_back(StringFormat("runs: %d", runs));
			strings.push_back(StringFormat("loopLen: %d", loopLen));
			strings.push_back(StringFormat("timeResample_usec/%d: %0.2f", loopLen, timeResample_usec/(float)loopLen));
			strings.push_back(StringFormat("time per 10k samples: %0.2f", (timeResample_usec/(float)loopLen)/(float)(oversampleCfg.numSamplesInput/10000.0f)));

			guibase* p = ctrl->guiFocused;
			int lvl = 0;
			while (p != NULL) {
				String s = "";
				if (lvl == 0) {
					s = "guiFocused: ";
				}
				for (int i = 0; i < lvl; i++) {
					s += "  ";
				}
				strings.push_back(s + p->getClassName());
				p = p->parent;
				lvl++;
			}

			int y1 = 160;
			int x = 5;

			setFont(vg, 26, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		//	String proj = StringFormat("Project: %s", StringAsCStr(ctrl->getProjectPath()));
		//	nvgText(vg, x, 0, StringAsCStr(proj), NULL);
			float lineh;
			nvgTextMetrics(vg, NULL, NULL, &lineh);
			int y = y1 + lineh;
			for (String& s : strings) {
				nvgText(vg, x, y, StringAsCStr(s), NULL);
				y += lineh;
			}
//			for (String& s : g_debugStrings) {
//				nvgText(vg, x, y, StringAsCStr(s), NULL);
//				y += lineh;
//			}
			for (auto* gui : guis) {
				nvgSave(vg);
				gui->render(vg);
				nvgRestore(vg);
			}

//			g_debugStrings.clear();
		}
//		void layout() {
//			ivec2 cs = getSizeContent();
//			int q = 240;
//			colorPick.size = ivec2(q*2, q);
//			colorPick.pos = ivec2(cs.x/2-colorPick.size.x, cs.y-colorPick.size.y);
//			field.size = ivec2(320, 32);
//			field.pos = ivec2(0, 0);
//			textField.size = ivec2(320, 32);
//			textField.pos = ivec2(55, field.bottom()+120);
//			ctrTabbed->pos = { cs.x / 2, 0 };
//			ctrTabbed->size.x = cs.x/2;
//			ctrTabbed->size.y = cs.y;
//			ctrTabbed->layout();
//			colorPick.layout();
//			field.layout();
//			textField.layout();
//		}
//		void addStr(String str) {
//			g_debugStrings.push_back(std::move(str));
//		}
	};
	class ViewContainersProfile {
	public:
		gui_statusbar statusbar;
		guictr_test_resample ctrMain;
		Splitter splitterCenter;
		ViewContainersProfile(/*const*/ AppCtrl* const ctrl) : ctrMain(), splitterCenter(0, 0.9f)
		{
			splitterCenter.setMinMax(0.05f, 0.95f);
		}
#if USE_GUI_MENU
		guictr_base* getMenuCtr() {
			return nullptr;
		}
#endif
		void layout(int32_t winW, int32_t winH) {
			int winX = 0; int winY = 0;
			int winBottom = winH;
			int hDbg = splitterCenter.leftOrTop(winH);
			int hstatusBar = splitterCenter.rightOrBottom(winH);
			statusbar.size = { winW, hstatusBar };
			statusbar.pos = { winX, winBottom - hstatusBar };
			splitterCenter.pos = ivec2(winX, statusbar.pos.y - 5);
			splitterCenter.size = ivec2(winW, Splitter::SPLITTER_LAYOUT_THICKNESS);
			ctrMain.setSnapSides(ivec4(0, 0, 0, 1));
			ctrMain.pos = {winX, winY};
			ctrMain.size = {winW, hDbg};
		}
		void addTo(std::vector<guictr_base*>& v) {
			 v.push_back(&splitterCenter);
			 v.push_back(&statusbar);
			 v.push_back(&ctrMain);
		}
	};
	class guictr_main : public guictr_base {
	public:
		guictr_main() : guictr_base() {

		}
	};
	class ViewContainersOtherTest {
	public:
		gui_statusbar statusbar;
		guictr_main ctrMain;
		Splitter splitterCenter;
		ViewContainersOtherTest(/*const*/ AppCtrl* const ctrl) : ctrMain(), splitterCenter(0, 0.9f)
		{
			splitterCenter.setMinMax(0.05f, 0.95f);
		}
#if USE_GUI_MENU
		guictr_base* getMenuCtr() {
			return nullptr;
		}
#endif
		void layout(int32_t winW, int32_t winH) {
			int winX = 0; int winY = 0;
			int winBottom = winH;
			int hDbg = splitterCenter.leftOrTop(winH);
			int hstatusBar = splitterCenter.rightOrBottom(winH);
			statusbar.size = { winW, hstatusBar };
			statusbar.pos = { winX, winBottom - hstatusBar };
			splitterCenter.pos = ivec2(winX, statusbar.pos.y - 5);
			splitterCenter.size = ivec2(winW, Splitter::SPLITTER_LAYOUT_THICKNESS);
			ctrMain.setSnapSides(ivec4(0, 0, 0, 1));
			ctrMain.pos = {winX, winY};
			ctrMain.size = {winW, hDbg};
		}
		void addTo(std::vector<guictr_base*>& v) {
			 v.push_back(&splitterCenter);
			 v.push_back(&statusbar);
			 v.push_back(&ctrMain);
		}
	};

	template<typename T>
	class MiniAppCtrl : public AppCtrl
	{
		T* view = NULL;
		Menus menus;

		hires_timer_t timer;
		seq_rand rand;
	public:
		int32_t numCallsWaitEvents = 0;

		~MiniAppCtrl() {
			my_printf("~TestAppCtrl\n",0);
		}
		String lastKey;
		void focusReceived() {
		}
		void focusLost() {
	//		closeContextMenu();
		}

		void destroy()
		{
			if (!isOK) {
				return;
			}
			isOK = false;
			delete view;

			daw_tls::tlsinstance& tls = daw_tls::getTls();
			delete tls.waveform;
			delete tls.audioCache;
			tls.waveform = nullptr;
			tls.audioCache = nullptr;
		}

		void menuCommand(int cmd) {
			switch (cmd) {
			case CMD_EXIT:
				mainWindow->requestClose();
				break;

			}
		}
		void postInit() {
		}
		void initApp(int argc, char* argv[]) {
			daw_tls::tlsinstance& tls = daw_tls::getTls();
			int sampleRate = 44100;
			tls.audioCache = new audiocache(sampleRate);
			tls.waveform = new waveformrender();
		}
		bool init(window_main* window, NVGcontext* nanovg)
		{
			this->mainWindow = window;
			this->window = window;
			this->vg = nanovg;
			themes.loadThemes();

			view = new T(this);
			view->addTo(this->containers);
			for (guictr_base *ctr : containers) {
				ctr->setControl(this);
			}

			menus.file.type = ngui::menu_type::submenu;
			menus.file.title = "File";
			menus.file.addCommand(CMD_EXIT, "Quit");

			menubar.add(&menus.file);
			this->updateMenubar();
		#if !USE_GUI_MENU
			this->mainWindow->updateMenu();
		#endif


			isOK = true;
			return isOK;
		}

		void onTick()
		{
		//	double since = timer.getTimeDoubleReset();
			for (guictr_base *ctr : containers) {
				ctr->onTick(this);
			}
		//	if (isPlaying()) {
				mainWindow->requestRedraw();
		//	}
		}

		void relayout(int32_t w, int32_t h) {
			closeAllAppMenus();
			closeContextMenu();
			m_size = ivec2(w, h);
			view->layout(w, h);

			for (guictr_base *ctr : containers) {
				ctr->layout();
			}
		}
		void mouseMoved(ivec2 mousePos, ivec2 deltaPos) {
		#if USE_GUI_MENU
			if (ctxtmenu != NULL) {
				auto ctr = view->getMenuCtr();
				if (ctr) {
					MouseHitEvt evt = mouseHitEvt(MouseHitType::MOUSE_OVER);
					if (ctr->mouseHitTest(mousePos, evt)) {
					}
				}
				return;
			}
		#endif
			BaseCtrl::mouseMoved(mousePos, deltaPos);
		}

		bool processGlobalKeyevent(KeyEvent& event) {

			if (event.type == KeyEventType::K_PRESS) {
				lastKey = getKeyName(event.scancode);
				if (!lastKey.length()) {
					const char* ca = glfwGetKeyName(event.keyCode, event.scancode);
					if (ca) {
						lastKey = ca;

					}
				}
			}
			if (event.type != KeyEventType::K_RELEASE) {
			}
			return false;
		}

		bool mouseDownPre() {
			closeAllContextMenus();
			return true;
		}

		void setStatusText(String s) {
			view->statusbar.setTitle(s);
		}
	};
	static std::shared_ptr<AppCtrl> appctrl;
}

std::shared_ptr<AppCtrl> makeApp() {
//	MiniApp::appctrl = std::make_shared<MiniApp::MiniAppCtrl<MiniApp::ViewContainersGuiTest>>();
	MiniApp::appctrl = std::make_shared<MiniApp::MiniAppCtrl<MiniApp::ViewContainersProfile>>();
	return MiniApp::appctrl;
}

void deleteApp() {
	MiniApp::appctrl.reset();
}
int main(int argc, char* argv[]) {
	return startApplication(argc, argv);
}
