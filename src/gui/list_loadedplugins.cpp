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
};
class gui_pluginsloaded_list : public guictr_base {
	const int32_t heightTextField = HEIGHT_DEFAULT_INPUT;
	gui_textfield textField;
	gui_list listCtr;
	String curquery = "";
	std::vector<gui_pluginsloaded_list_entry> entries;
	uint64_t lastUpdate = 0;
public:
	gui_pluginsloaded_list() : guictr_base() {
		setBackgroundRendered(true);
		listCtr.padding = 0;
		add(&textField);
		add(&listCtr);
		textField.setCallback([this](const String& str) {
			curquery = str;
			update();
			return true;
		});
		textField.setPlaceholder("Search");
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
		MainCtrl *ctrl = MainCtrl::get();
		std::vector<effectbase*> effects;
		vsthost::getInstance()->getAllInstances(effects);
		std::vector<gui_list_entry*> _newList;
		entries.clear();

		//TODO: use saferef
		for (effectbase* eff : effects) {
			SafeRef<effectbase> safeRef = eff->makeSafeRef();
			gui_pluginsloaded_list_entry* g = new gui_pluginsloaded_list_entry(safeRef);
			_newList.push_back(g);
		}
		listCtr.setList(_newList);
		layout();
	}
	bool mouseHitTest(ivec2 mpos, MouseHitEvt& evt) override {
		if (this->contains(mpos)) {
			ivec2 localMouse = this->toContainerSpace(mpos);
			for (guibase* gui : guis) {
				if (gui->mouseHitTest(localMouse, evt)) {
//					my_printf("clicked on %s %d\n", gui->getClassName().c_str(), (int) h);
					return true;
				}
			}
		}
		return false;
	}
	void layout() {
		ivec2 cs = getSizeContent();
		textField.size = ivec2(cs.x, heightTextField);
		textField.pos = ivec2(0, 0);
		listCtr.pos = ivec2(0, heightTextField);
		listCtr.size = ivec2(cs.x, cs.y-heightTextField);
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
		textField.render(vg);
		listCtr.render(vg);
	}
};

guictr_base* makeGuiPluginsLoadedList() {
	return new gui_pluginsloaded_list();
}
