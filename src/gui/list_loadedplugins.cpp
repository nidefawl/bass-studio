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
			nvgTextAlign(vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
			String str = StringFormat("%.2f%%", _entry->fTimePercentBlockProcess*100.0);
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
		float lineh;
		setFont(vg, 12, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
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
		printL("maxLatencyAudioMidi", StringFormat("%lld", stats.maxLatencyAudioMidi));
		printL("maxLatencyReturn", StringFormat("%lld", stats.maxLatencyReturn));
		printL("latencyToMaster", StringFormat("%lld", stats.latencyToMaster));
		printL("playback_state", StringFormat("%lld", static_cast<int32_t>(state)));
		audiothread_ringbuffer_t& ringbuffer = vsthost::getInstance()->getRingBuffer();
		printL("rinbuffer.writepos", StringFormat("%lld", ringbuffer.writePos));
		printL("rinbuffer.readpos", StringFormat("%lld", ringbuffer.readPos));
		printL("inputBufferUnderuns", StringFormat("%lld", stats.inputBufferUnderuns));
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
class gui_pluginsloaded_list : public guictr_base {
//	const int32_t heightTextField = HEIGHT_DEFAULT_INPUT;
//	gui_textfield textField;
	gui_stats_list list;
	guictr_scrollbar scrollTop;
	gui_list listCtr;
	String curquery = "";
	uint64_t lastUpdate = 0;
public:
	gui_pluginsloaded_list() : guictr_base() {
		setBackgroundRendered(true);
		scrollTop.add(&list);
		padding = 4;
		listCtr.setRowHeight(14);
		listCtr.padding = 0;
		list.padding = 0;
		scrollTop.padding = 0;
		scrollTop.maxHeight = -1;
//		add(&textField);
		add(&scrollTop);
		add(&listCtr);
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
	std::vector<gui_pluginsloaded_list_entry*> listEntriesLoadedPlugins;
	void buttonClicked(guibase* button) {
		if (STL_CONTAINS(listEntriesLoadedPlugins, button)) {
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
	void update() {
		std::vector<effectbase*> effects;
		vsthost::getInstance()->getAllInstances(effects);
		std::stable_sort(effects.begin(), effects.end(), [](const effectbase* ptrA, const effectbase* ptrB){
			return ptrA->fTimePercentBlockProcess > ptrB->fTimePercentBlockProcess;
		});
		std::vector<gui_list_entry*> _newList;
		std::vector<gui_pluginsloaded_list_entry*> _newListLoadedPlugins;

		//TODO: use saferef
		for (effectbase* eff : effects) {
			SafeRef<effectbase> safeRef = eff->makeSafeRef();
			gui_pluginsloaded_list_entry* g = new gui_pluginsloaded_list_entry(safeRef);
			_newList.push_back(g);
			_newListLoadedPlugins.push_back(g);
		}
		listCtr.setList(_newList);
		listEntriesLoadedPlugins = _newListLoadedPlugins;
		layout();
	}
	void layout() {
		ivec2 cs = getSizeContent();
		scrollTop.pos = ivec2(0, 0);
		listCtr.pos = ivec2(0, cs.y/2);
		scrollTop.size = ivec2(cs.x, cs.y/2);
		listCtr.size = ivec2(cs.x, cs.y/2);
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
