#include "exceptions.h"
#include "str_util.h"
#include "commands.h"
#include "config.h"
#include "platform.h"
#include "exceptions.h"

#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_map>

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>

template <class Archive>
void serialize(Archive& archive, KeyCombo& m) {
    archive(cereal::make_nvp("keycode", m.keyCode), cereal::make_nvp("mod", m.keyMod), cereal::make_nvp("keychar", m.keyChar));
}
namespace DAW::UI {


template <class Archive>
void serialize(Archive& archive, KeybindsSnapshot& m) {
    archive(cereal::make_nvp("version", m.version), cereal::make_nvp("keybinds", m.keyCombos));
}
KeybindsSnapshot loadKeybindsFile() {
    Stringstream ss;
    String cwdPathTheme = App::Platform::toUserdataPath(KEYBIND_SETTINGS_FILENAME);
    std::ifstream file(cwdPathTheme, std::ifstream::in);
    if (file) {
        ss << file.rdbuf();
        std::streampos length = file.tellg();
        if (length > 10) {
            KeybindsSnapshot tmpSettings;
            cereal::JSONInputArchive ar(ss);
            ar(tmpSettings);
            return tmpSettings;
        }
    }
    throw FileIOException("Failed reading keybinds file " + cwdPathTheme);
}
void saveKeybindsFile(KeybindsSnapshot& _settings) {
    String cwdPathTheme = App::Platform::toUserdataPath(KEYBIND_SETTINGS_FILENAME);
    std::ofstream file;
    file.exceptions(~std::ofstream::goodbit);
    file.open(cwdPathTheme, std::ofstream::out);
    cereal::JSONOutputArchive ar(file);
    ar(_settings);
}
}
