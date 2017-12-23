#pragma once
#include <nanovg.h>
#include "mainctrl.h"
#include "gui.h"
#include "str_util.h"
#include "guicolors.h"
#include "exceptions.h"
#include "mouse.h"
#include "textfield.h"
#include "list.h"
#include "../host/plugindatabase.h"

using glm::vec2;
using glm::ivec2;
using glm::vec4;
using glm::ivec4;
class gui_pluginlist_entry : public gui_list_entry {
public:
	const pluginentry_t entry;
	gui_pluginlist_entry(const pluginentry_t _entry) : gui_list_entry(), entry(_entry) {
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
};
class guictr_pluginlibrary : public guictr_base {
	const int32_t heightTextField = 30;
	gui_textfield textField;
	gui_list pluginListCtr;
	String curquery = "";
	std::vector<pluginentry_t> pluginsLibList;
public:
	guictr_pluginlibrary() : guictr_base() {
		pluginListCtr.padding = 0;
		add(&textField);
		add(&pluginListCtr);
//		textField.setCallback([this](const String& str) {
//			curquery = str;
//			update();
//			return true;
//		});
		textField.setPlaceholder("Search");
	}
	~guictr_pluginlibrary() {
		remove(&pluginListCtr);
		remove(&textField);
	}
	void update() {
		MainCtrl *ctrl = MainCtrl::get();
		std::vector<gui_list_entry*> _newList;

		pluginsLibList.clear();
		ctrl->plugindb.query(curquery, pluginsLibList);

		for (pluginentry_t& entry : pluginsLibList) {
			gui_pluginlist_entry* g = new gui_pluginlist_entry(entry);
			_newList.push_back(g);
		}
		pluginListCtr.setList(_newList);
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
		pluginListCtr.pos = ivec2(0, heightTextField);
		pluginListCtr.size = ivec2(cs.x, cs.y-heightTextField);
		for (guibase* gui : guis) {
			gui->layout();
		}
	}
	virtual void render(NVGcontext* vg) {
		renderBackground(vg);
		if (!setScissorTransform(vg)) {
			return;
		}
		textField.render(vg);
		pluginListCtr.render(vg);
	}
};
