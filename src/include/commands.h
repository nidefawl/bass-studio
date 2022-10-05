#pragma once
#include <vector>
#include "str_util.h"
#include "keyboard.h"
#include "mouse.h"
#include "event.h"

namespace DAW::UI {
struct CommandDesc {
    String name;
    String description;
    String shortcut;
    int32_t iconId = -1;
};
struct Command {
    GlobalCommandType type = GlobalCommandType::CMD_NONE;
    CommandDesc desc;
    std::vector<KeyCombo> keyCombos;
    int group = 0;
    String toString() const {
        return desc.name;
    }
};
class CommandManager {
    std::vector<Command> commands;
    void updateCommandShortcuts();
public:
    void init();
    Command* matchKeyCombo(const KeyEvent& evt);
    Command* getCommand(GlobalCommandType type);
};
} // namespace DAW::UI