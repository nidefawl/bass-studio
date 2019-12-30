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
#include "button.h"
#include "host/audio_host.h"

class gui_pluginsloaded_list_entry : public gui_list_entry {
	SafeRef<effectbase> ref;
public:
	SafeRef<effectbase>& getRef() {
		return ref;
	}
	gui_pluginsloaded_list_entry(SafeRef<effectbase> _ref) : gui_list_entry(), ref(_ref) {
		auto* _entry = safeRefGet(ref);
		dbgassert(_entry);
		icon = _entry->isSynth ? ICON_SYNTH : ICON_EFFECT;
	}
	String getText() override {
		auto* _entry = safeRefGet(ref);
		if (!_entry)
			return "<invalid reference>";
		return _entry->getName();
	}
	bool isSynth() {
		auto* _entry = safeRefGet(ref);
		if (!_entry)
			return false;
		return _entry->isSynth;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
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
		auto* _entry = safeRefGet(ref);
		if (_entry) {
			host_stats_reducted_t stats;
			auto host = vsthost::getInstance();
			host->getShortStats(stats);
			float fPercentLoad = stats.timeProcess <= 0 ? 0 : _entry->timeProcess*100.0f / stats.timeProcess;
			nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
			String str = StringFormat("%.2f%%", fPercentLoad);
			nvgText(vg, size.x-spacing, rowHeight / 2, StringAsCStr(str), NULL);
		}
		nvgTranslate(vg, -pos.x, -pos.y);
	}
};
class gui_stats_list : public guictr_base
{
	int32_t minHTop = 66;
public:
	gui_stats_list() : guictr_base() {
		setBackgroundRendered(true);
	}
	~gui_stats_list() {

	}
	virtual void render(NVGcontext* vg) {
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
		int x = 5;
		int x2 = getSizeContent().x-x;
		int y = 5;
		host_stats_t stats;
		playback_state state;
		{
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			state = MainCtrl::getPlayThread()->getState();
			vsthost::getInstance()->getStats(stats);
		}
//		const int fontSize = 12;
		int32_t fontSize = 14;
		int32_t w = size.x/128;
		fontSize += w*4;
		float lineh;
		setFont(vg, fontSize, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		nvgTextMetrics(vg, NULL, NULL, &lineh);

		auto printL = [&](const char* caption, const String& str) {
			nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
			nvgText(vg, x, y, caption, NULL);
			nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
			nvgText(vg, x2, y, StringAsCStr(str), NULL);
			y += lineh;
		};
		printL("Usage", StringFormat("%.2f%%", stats.usage*100.0));
		printL("blocksProcessed", StringFormat("%d", stats.blocksProcessed));
		printL("samplesProcessed", StringFormat("%d", stats.samplesProcessed));
		printL("timeLastBlock", StringFormat("%lld", stats.timeLastBlock));
		printL("playback_state", StringFormat("%lld", static_cast<int32_t>(state)));
		audiothread_ringbuffer_t& ringbuffer = vsthost::getInstance()->getRingBuffer();
		printL("input q len", StringFormat("%d", stats.inputQueueLen));
		printL("output q len", StringFormat("%d", stats.outputQueueLen));
		printL("INPUT  resampler", StringFormat("%d samples|%d blocks", stats.resamplerInNumSamples, stats.resamplerInNumBlocks));
		printL("OUTPUT resampler", StringFormat("%d samples|%d blocks", stats.resamplerOutNumSamples, stats.resamplerOutNumBlocks));
		printL("output q len", StringFormat("%d", stats.outputQueueLen));
		printL("inputBufferUnderuns", StringFormat("%d", stats.inputBufferUnderuns));
		auto audioHost = audiohost::getInstance();
		printL("outputBufferUnderuns", StringFormat("%u", audioHost ? audioHost->bufferUnderuns : 0));
		printL("inputBufferOverrun", StringFormat("%u", audioHost ? audioHost->inputBufferUnderuns : 0));
		printL("audioCallback tDelta usec", StringFormat("%d", audioHost ? audioHost->audioCallbackInvocationDelay_usec : 0));
		for (auto& entry : stats.timings) {
			printL(StringAsCStr(entry.first), StringFormat("%lld", entry.second));
		}
		minHTop = y+lineh;

	}
	virtual void determineSize(ivec2& prefSize) override {
		prefSize.x = math::max(100, prefSize.x);
		prefSize.y = math::max(math::max(minHTop, 100), prefSize.y);
	}
};
class gui_list_plugins : public guictr_base {
	std::vector<gui_pluginsloaded_list_entry*>& entries;
public:
	gui_list list;

	guibutton btnLoadAll;
	gui_list_plugins(std::vector<gui_pluginsloaded_list_entry*>& _entries) : guictr_base(), entries(_entries) {

		setBackgroundRendered(true);
		list.padding = 0;
		list.setBackgroundRendered(false);
		list.setRowHeight(14);
		add(&list);
		add(&btnLoadAll);
		btnLoadAll.setLabel("Load all");
	}

	void layout() {
		int32_t rowHeight = 14;
		auto cs = getSizeContent();
		int32_t w = cs.x/128;
		rowHeight += w*4;
		list.setRowHeight(rowHeight);
//		const int32_t hpt = theme->get(GuiConstant::CONST_FIXED_TITLE_HEIGHT);
		const int32_t inset = math::min(6, theme->get(GuiConstant::CONST_LAYOUT_MARGIN));
		const int32_t TRACK_HEIGHT_STEP = theme->get(GuiConstant::CONST_TRACK_HEIGHT_STEP);
		list.pos = {inset, TRACK_HEIGHT_STEP+inset};
		list.size = {cs.x-inset*2, cs.y-TRACK_HEIGHT_STEP-inset*2};
		btnLoadAll.pos = {inset, inset};
		btnLoadAll.size = {cs.x-inset*2, TRACK_HEIGHT_STEP-inset*2};
		for (auto* g : guis) {
			g->layout();
		}
	}
	void buttonClicked(guibase* button) {
		if (&btnLoadAll == button) {

			log_printf("load all deferred\n", 0);
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			auto* host = vsthost::getInstance();
    		std::vector<effectbase*> pluginsDeferred;
    		host->getDeferredEffects(pluginsDeferred);
    		my_printf("loading %d plugins\n", pluginsDeferred.size());
    		for (auto plugin : pluginsDeferred) {
        		my_printf("activate %s\n", StringAsCStr(plugin->sName));
        		effectbase* effectLoaded = nullptr;
    			host->activateDeferred(plugin, &effectLoaded);
//            			if (effectLoaded) {
//            				effectLoaded->show();
//            			}
    		}
		} else if (STL_CONTAINS(entries, button)) {
			gui_pluginsloaded_list_entry* entry = dynamic_cast<gui_pluginsloaded_list_entry*>(button);
			auto* effectbase = safeRefGet(entry->getRef());
			if (effectbase) {
				track_t* tr = effectbase->getTrack();
				if (tr) {
					MainCtrl::get()->setSelectedTrack(tr);
					MainCtrl::get()->showPluginView();
//					MainCtrl::get()->getPluginCtr()->makeVisibleTo(effectbase); //scrollTo
				}
			}

		}
	}
	virtual void render(NVGcontext* vg) {
		if (isBackgroundRendered()) {
			renderBackground(vg);
		}
		if (!setScissorTransform(vg)) {
			return;
		}
		for (auto* g : guis) {
			if (g->isVisible()) {
				nvgSave(vg);
				g->render(vg);
				nvgRestore(vg);
			}
		}

	}
	~gui_list_plugins() {
		removeGuis();
	}
};
class gui_pluginsloaded_list : public guictr_base {
//	const int32_t heightTextField = HEIGHT_DEFAULT_INPUT;
//	gui_textfield textField;
	gui_stats_list textStats;
	guictr_scrollbar scrollTop;
	gui_list_plugins listCtr;
	gui_list_plugins listDeferredCtr;
	String curquery = "";
	uint64_t lastUpdate = 0;
	std::vector<gui_pluginsloaded_list_entry*> listEntriesLoadedPlugins;
	std::vector<gui_pluginsloaded_list_entry*> listEntriesDef;
public:
	gui_pluginsloaded_list() : guictr_base(), listCtr(listEntriesLoadedPlugins), listDeferredCtr(listEntriesDef) {
		setBackgroundRendered(false);
		padding = 0;
		margin = 0;
		scrollTop.add(&textStats);
		scrollTop.maxHeight = -1;
		add(&scrollTop);
		add(&listCtr);
		add(&listDeferredCtr);
//		textField.setCallback([this](const String& str) {
//			curquery = str;
//			update();
//			return true;
//		});
//		textField.setPlaceholder("Search");
	}
	~gui_pluginsloaded_list() {
		removeGuis();
	}
	void onTick(AppCtrl* ctrl) override {
		guictr_base::onTick(ctrl);
		uint64_t u = getTimeMillis();
		if (u-lastUpdate > 1000) {
			lastUpdate = u;
			update();
		}
	}
	void update() {
		ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
		std::vector<effectbase*> effects;
		vsthost::getInstance()->getAllInstances(effects);
		std::stable_sort(effects.begin(), effects.end(), [](const effectbase* ptrA, const effectbase* ptrB){
			return ptrA->timeProcess > ptrB->timeProcess;
		});
		std::vector<gui_list_entry*> _newList;
		std::vector<gui_list_entry*> _newListDef;
		std::vector<gui_pluginsloaded_list_entry*> _newListLoadedPlugins;
		std::vector<gui_pluginsloaded_list_entry*> _newListDefPlugins;
		std::vector<effectbase*> deferredEffects;
		auto host = vsthost::getInstance();
		host->getDeferredEffects(deferredEffects);
		std::stable_sort(deferredEffects.begin(), deferredEffects.end(), [](const effectbase* ptrA, const effectbase* ptrB){
			if (ptrA->sName == ptrB->sName)
				return (size_t)ptrA > (size_t)ptrB;
			return ptrA->sName > ptrB->sName;
		});
		for (effectbase* eff : deferredEffects) {
			SafeRef<effectbase> safeRef = eff->makeSafeRef();
			gui_pluginsloaded_list_entry* g = new gui_pluginsloaded_list_entry(safeRef);
			_newListDef.push_back(g);
			_newListDefPlugins.push_back(g);
		}
		//TODO: use saferef
		for (effectbase* eff : effects) {
			SafeRef<effectbase> safeRef = eff->makeSafeRef();
			gui_pluginsloaded_list_entry* g = new gui_pluginsloaded_list_entry(safeRef);
			_newList.push_back(g);
			_newListLoadedPlugins.push_back(g);
		}
		listCtr.list.setList(_newList);
		listDeferredCtr.list.setList(_newListDef);
		listEntriesLoadedPlugins = _newListLoadedPlugins;
		listEntriesDef = _newListDefPlugins;
		layout();
	}
	void layout() {
		ivec2 cs = getSizeContent();
		scrollTop.pos = ivec2(0, 0);
		listCtr.pos = ivec2(0, cs.y/3);
		listDeferredCtr.pos = ivec2(0, cs.y*2/3);
		scrollTop.size = ivec2(cs.x, cs.y/3);
		listCtr.size = ivec2(cs.x, cs.y/3);
		listDeferredCtr.size = ivec2(cs.x, cs.y/3);
		scrollTop.determineSize(scrollTop.size);
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
		for (auto* g : guis) {
			nvgSave(vg);
			g->render(vg);
			nvgRestore(vg);
		}

	}
};

guictr_base* makeGuiPluginsLoadedList() {
	return new gui_pluginsloaded_list();
}
