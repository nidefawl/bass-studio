#pragma once
#include <utility>
#include <vector>
#include "str_util.h"
#include "exceptions.h"

struct menucmd_t {
    int command = 0;
    String arg1;
    int argInt = 0;
};

inline menucmd_t CMD_NOARG(int cmd) {
    return menucmd_t{ cmd, "", 0 };
}

inline menucmd_t CMD_NUMBER_ARG(int cmd, int argInt) {
    return menucmd_t{ cmd, "", argInt };
}

namespace ngui {

    enum menu_type {
        command,
        seperator,
        submenu
    };

    struct Menu {
    public:
        menu_type type = menu_type::command;
        menucmd_t command;
        String title;
        std::vector<Menu> entries;
        std::vector<Menu*> children;
        bool disabled = false;
        bool checked  = false;
        Menu* parent  = nullptr;
        int icon      = -1;

    private:
        Menu& makeChild_() {
            if (entries.empty()) {
                entries.reserve(128);
            } else if (entries.capacity() - entries.size() < 1) {
                throw applogicexception("out of menu space");
            }
            Menu m;
            entries.push_back(m);
            return entries.back();
        }

    public:
        void addCommand(menucmd_t menuCmd, String cmdTitle, int cmdIcon = -1) {
            Menu& m   = makeChild_();
            m.type    = menu_type::command;
            m.command = std::move(menuCmd);
            m.title   = std::move(cmdTitle);
            m.icon    = cmdIcon;
            add(&m);
        }
        void remove(Menu* m) {
            auto it = std::find(children.begin(), children.end(), m);
            if (it != children.end()) {
                (*it)->parent = nullptr;
                children.erase(it);
            }
        }
        void addSeperator() {
            Menu& m           = makeChild_();
            m.type            = menu_type::seperator;
            m.command.command = 0;
            m.title           = "";
            add(&m);
        }
        void add(Menu* m) {
            children.push_back(m);
            m->parent = this;
        }
        Menu* getByName(const String& name) {
            for (Menu* m : children) {
                if (m->title == name)
                    return m;
            }
            return nullptr;
        }
        Menu* getByCmd(int cmd) {
            for (Menu* m : children) {
                if (m->type == menu_type::command) {
                    if (m->command.command == cmd) {
                        return m;
                    }
                }
            }
            return nullptr;
        }
        void clear() {
            children.clear();
            entries.clear();
        }
    };

    struct MenuBar : Menu {
        bool disableAll = false;
    };
}
