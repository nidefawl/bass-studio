#pragma once
#include <nanovg.h>
#include "../host/mainctrl.h"
#include "gui.h"
#include "str_util.h"
#include "guicolors.h"
#include "exceptions.h"
#include "mouse.h"
#include "textfield.h"
#include "list.h"
#include "../host/plugindatabase.h"
#include "../host/vst_host.h"
#include "modules.h"

class effectbase;
class gui_pluginlist_entry : public gui_list_entry {
public:
	gui_pluginlist_entry() {

	}
	virtual ~gui_pluginlist_entry() {

	}
	virtual effectbase* makeInstance() = 0;
	virtual bool isSynth() = 0;
};
class gui_vstpluginlist_entry : public gui_pluginlist_entry {
public:
	const pluginentry_t entry;
	gui_vstpluginlist_entry(const pluginentry_t _entry) : gui_pluginlist_entry(), entry(_entry) {
		icon = _entry.isSynth ? ICON_SYNTH : ICON_EFFECT;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
		target->pluginEntryDragMove(this, mousepos);
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
		target->pluginEntryDragRelease(this, mousepos);
	}
	String getText() override {
		return entry.name;
	}
	effectbase* makeInstance() override;
	bool isSynth() override {
		return entry.isSynth;
	}
};
class guictr_pluginlibrary : public guictr_base {
	const int32_t heightTextField = HEIGHT_DEFAULT_INPUT;
	gui_textfield textField;
	gui_list pluginListCtr;
	String curquery = "";
	std::vector<pluginentry_t> pluginsLibList;
public:
	guictr_pluginlibrary() : guictr_base() {
		setBackgroundRendered(true);
		pluginListCtr.padding = 0;
		add(&textField);
		add(&pluginListCtr);
		textField.setCallback([this](const String& str) {
			curquery = str;
			update();
			return true;
		});
		textField.setPlaceholder("Search");
	}
	~guictr_pluginlibrary() {
		std::vector<gui_list_entry*> _newList;
		pluginListCtr.setList(_newList);
		remove(&pluginListCtr);
		remove(&textField);
	}
	void update() {
		MainCtrl *ctrl = MainCtrl::get();
		std::vector<gui_list_entry*> _newList;
		pluginsLibList.clear();
		ctrl->plugindb.query(curquery, pluginsLibList);

		for (pluginentry_t& entry : pluginsLibList) {
			gui_pluginlist_entry* g = new gui_vstpluginlist_entry(entry);
			_newList.push_back(g);
		}
		pluginListCtr.setList(_newList);
		layout();
	}
	void layout() {
		ivec2 cs = getSizeContent();
		textField.size = ivec2(cs.x, heightTextField);
		textField.pos = ivec2(0, 0);
		pluginListCtr.pos = ivec2(0, heightTextField);
		pluginListCtr.size = ivec2(cs.x, cs.y-heightTextField);
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
		pluginListCtr.render(vg);
	}
};
struct module_desc_t {
	int moduleType;
	int moduleId;
	String name;
	bool isSynth;
};
class gui_modulelist_entry : public gui_pluginlist_entry {
public:
	const module_desc_t entry;
	gui_modulelist_entry(const module_desc_t _entry) : gui_pluginlist_entry(), entry(_entry) {
		icon = _entry.isSynth ? ICON_SYNTH : ICON_EFFECT;
	}
	void dragMoveOn(guibase* target, ivec2 mousepos) override {
		target->pluginEntryDragMove(this, mousepos);
	}
	void dragReleaseOn(guibase* target, ivec2 mousepos) override {
		target->pluginEntryDragRelease(this, mousepos);
	}
	String getText() override {
		return entry.name;
	}
	effectbase* makeInstance() override;
	bool isSynth() override {
		return entry.isSynth;
	}
};
class guictr_modulelibrary : public guictr_base {
	const int32_t heightTextField = 30;
	gui_textfield textField;
	gui_list pluginListCtr;
	String curquery = "";
	std::vector<module_desc_t> effectEntries;
public:
	guictr_modulelibrary() : guictr_base() {
		setBackgroundRendered(true);
		effectEntries.push_back(module_desc_t{PLUGIN_TYPE_EMPTY, 0, "Empty", false});
		effectEntries.push_back(module_desc_t{PLUGIN_TYPE_GROUP, 0, "Group", false});
		auto host = vsthost::getInstance();
		if (host) {
			std::vector<builtin_module_reg_t>& vecReg = host->getBuiltinModuleRegistry();
			for (auto& reg : vecReg) {
				effectEntries.push_back(module_desc_t{PLUGIN_TYPE_INTERNAL_EFFECT, reg.id, reg.name, reg.isSynth});
			}
		}
		pluginListCtr.padding = 0;
		add(&textField);
		add(&pluginListCtr);
		textField.setCallback([this](const String& str) {
			curquery = str;
			update();
			return true;
		});
		textField.setPlaceholder("Search");
	}
	~guictr_modulelibrary() {
		removeGuis();
	}
	void update() {
		std::vector<gui_list_entry*> _newList;
		for (auto& t : effectEntries) {
			if (ci_find_substr(t.name, curquery) >= 0) {
				gui_modulelist_entry* g = new gui_modulelist_entry(t);
				_newList.push_back(g);
			}
		}
		pluginListCtr.setList(_newList);
		layout();
	}
	void layout() {
		ivec2 cs = getSizeContent();
		textField.size = ivec2(cs.x, heightTextField);
		textField.pos = ivec2(0, 0);
		pluginListCtr.pos = ivec2(0, heightTextField);
		pluginListCtr.size = ivec2(cs.x, cs.y-heightTextField);
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
		pluginListCtr.render(vg);
	}
};
