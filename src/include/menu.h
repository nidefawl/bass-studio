#pragma once
#include <vector>
#include "str_util.h"

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
		std::vector<Menu*> entries;
		bool disabled = false;
		bool checked = false;
		Menu* parent = NULL;
		void addCommand(int cmd, String title) {
			Menu* m = new Menu();
			m->type = menu_type::command;
			m->command = cmd;
			m->title = title;
			add(m);
		}
		void addSeperator() {
			Menu* m = new Menu();
			m->type = menu_type::seperator;
			m->command = 0;
			m->title = "";
			add(m);
		}
		void add(Menu* m) {
			entries.push_back(m);
			m->parent = this;
		}
		Menu* getByName(String name) {
			for (Menu* m : entries){
				if (m->title == name)
					return m;
			}
			return NULL;
		}
		Menu* getByCmd(int cmd) {
			for (Menu* m : entries){
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
