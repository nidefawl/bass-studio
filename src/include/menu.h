#pragma once
#include <vector>
#include "str_util.h"
#include "exceptions.h"

namespace ngui {

	enum menu_type {
		command,
		seperator,
		submenu
	};
	struct Menu {
	public:
		menu_type type;
		int command;
		String title;
		std::vector<Menu> entries;
		std::vector<Menu*> children;
		bool disabled = false;
		bool checked = false;
		Menu* parent = NULL;
		int icon = -1;
	private:
		Menu& makeChild_() {
			if (entries.size() == 0) {
				entries.reserve(16);
			} else if (entries.capacity()-entries.size() < 1) {
				throw new applogicexception("out of menu space");
			}
			Menu m;
			entries.push_back(m);
			return entries.back();
		}
	public:
		void addCommand(int cmd, String title, int icon = -1) {
			Menu& m = makeChild_();
			m.type = menu_type::command;
			m.command = cmd;
			m.title = title;
			m.icon = icon;
			add(&m);
		}
		void remove(Menu* m) {
			auto it = std::find(children.begin(), children.end(), m);
			if (it != children.end()) {
				(*it)->parent = NULL;
				children.erase(it);
			}
		}
		void addSeperator() {
			Menu& m = makeChild_();
			m.type = menu_type::seperator;
			m.command = 0;
			m.title = "";
			add(&m);
		}
		void add(Menu* m) {
			children.push_back(m);
			m->parent = this;
		}
		Menu* getByName(String name) {
			for (Menu* m : children){
				if (m->title == name)
					return m;
			}
			return NULL;
		}
		Menu* getByCmd(int cmd) {
			for (Menu* m : children){
				if (m->type == menu_type::command) {
					if (m->command == cmd) {
						return m;
					}
				}
			}
			return NULL;
		}
	};
	struct MenuBar : Menu {
		bool disableAll = false;
	};

}
