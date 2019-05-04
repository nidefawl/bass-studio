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

class gui_pluginsloaded_list_entry : public gui_list_entry {
	SafeRef<effectbase> ref;
public:
	gui_pluginsloaded_list_entry(SafeRef<effectbase> _ref) : gui_list_entry(), ref(_ref) {
		auto* _entry = safeRefGet(ref);
		assert(_entry);
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
class gui_pluginsloaded_list : public guictr_base {
//	const int32_t heightTextField = HEIGHT_DEFAULT_INPUT;
//	gui_textfield textField;
	gui_list listCtr;
	String curquery = "";
	uint64_t lastUpdate = 0;
public:
	gui_pluginsloaded_list() : guictr_base() {
		setBackgroundRendered(true);
		listCtr.padding = 0;
//		add(&textField);
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
	void update() {
		std::vector<effectbase*> effects;
		vsthost::getInstance()->getAllInstances(effects);
		std::stable_sort(effects.begin(), effects.end(), [](const effectbase* ptrA, const effectbase* ptrB){
			return ptrA->fTimePercentBlockProcess > ptrB->fTimePercentBlockProcess;
		});
		std::vector<gui_list_entry*> _newList;

		//TODO: use saferef
		for (effectbase* eff : effects) {
			SafeRef<effectbase> safeRef = eff->makeSafeRef();
			gui_pluginsloaded_list_entry* g = new gui_pluginsloaded_list_entry(safeRef);
			_newList.push_back(g);
		}
		listCtr.setList(_newList);
		layout();
	}
	void layout() {
		ivec2 cs = getSizeContent();
//		textField.size = ivec2(cs.x, heightTextField);
//		textField.pos = ivec2(0, 0);
		listCtr.pos = ivec2(0, cs.y/3);
		listCtr.size = ivec2(cs.x, cs.y-listCtr.pos.y);
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
		int x = 5;
		int x2 = getSizeContent().x-x;
		int y = 5;
		host_stats_t stats;
		{
			ThreadLock lock = MainCtrl::getPlayThread()->lockThread();
			vsthost::getInstance()->getStats(stats);
		}
		float lineh;
		String str;
		setFont(vg, 26, G_WHITE, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		nvgTextMetrics(vg, NULL, NULL, &lineh);

		str = StringFormat("%.2f%%", stats.usage*100.0);
		nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		nvgText(vg, x, y, "Usage", NULL);
		nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
		nvgText(vg, x2, y, StringAsCStr(str), NULL);
		y += lineh;

		str = StringFormat("%d", stats.blocksProcessed);
		nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		nvgText(vg, x, y, "blocksProcessed", NULL);
		nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
		nvgText(vg, x2, y, StringAsCStr(str), NULL);
		y += lineh;
		str = StringFormat("%d", stats.samplesProcessed);
		nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		nvgText(vg, x, y, "samplesProcessed", NULL);
		nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
		nvgText(vg, x2, y, StringAsCStr(str), NULL);
		y += lineh;
		str = StringFormat("%lld", stats.timeLastBlock);
		nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
		nvgText(vg, x, y, "timeLastBlock", NULL);
		nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
		nvgText(vg, x2, y, StringAsCStr(str), NULL);
		y += lineh;
		for (auto& entry : stats.timings) {
			str = StringFormat("%lld", entry.second);
			nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_LEFT);
			nvgText(vg, x, y, StringAsCStr(entry.first), NULL);
			nvgTextAlign(vg, NVG_ALIGN_TOP | NVG_ALIGN_RIGHT);
			nvgText(vg, x2, y, StringAsCStr(str), NULL);
			y += lineh;
		}

		listCtr.render(vg);
	}
};

guictr_base* makeGuiPluginsLoadedList() {
	return new gui_pluginsloaded_list();
}
