#include "themefile.h"
#include "str_util.h"
#include "color_util.h"
#include "config.h"
#include "theme.h"
#include "guiconstant.h"
#include "guicolors.h"
#include "msgbox.h"
#include "renderresources.h"
#include "assert_dbg.h"
#include "platform.h"

#include <cmath>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/polymorphic.hpp>

using namespace std;
using namespace cereal;


namespace GuiColor {
    constant_t getConstantById(int32_t id);
    constant_t getConstantByName(String name);
} // namespace GuiColor
namespace GuiConstant {
    constant_t getConstantById(int32_t id);
    constant_t getConstantByName(String name);
} // namespace GuiConstant

struct theme_data {
    std::unordered_map<String, int32_t> mapColors;
    std::unordered_map<String, int32_t> mapProperties;
    std::unordered_map<String, String> mapFonts;
};

void storeThemeData(guitheme_t const& m, theme_data& out) {
    for (auto it = m.mapColors.begin(); it != m.mapColors.end(); ++it) {
        int32_t key = it->first;
        auto c      = GuiColor::getConstantById(key);
        if (c.idx <= 0) continue;
        out.mapColors[c.name] = it->second;
    }
    for (auto it = m.mapProperties.begin(); it != m.mapProperties.end(); ++it) {
        int32_t key               = it->first;
        GuiConstant::constant_t c = GuiConstant::getConstantById(key);
        if (c.idx <= 0) continue;
        out.mapProperties[c.name] = it->second;
    }
    for (auto it = m.mapFonts.begin(); it != m.mapFonts.end(); ++it) {
        int32_t key           = it->first;
        UIFont::font_type_t c = UIFont::getConstantById(key);
        if (c.idx <= 0) continue;
        out.mapFonts[c.name] = it->second.name;
    }
}
void loadThemeData(theme_data& data, guitheme_t& out) {
    for (auto it = data.mapColors.begin(); it != data.mapColors.end(); ++it) {
        String key = it->first;
        auto c     = GuiColor::getConstantByName(key);
        if (c.idx <= 0) {
            continue;
        }
        out.mapColors[c.idx] = it->second;
        if (c.idx < out.vecNVGColors.size()) {
            out.vecNVGColors[c.idx] = rgbaToNvg(it->second);
        } else {
            dbgassert(0);
        }
    }
    for (auto it = data.mapProperties.begin(); it != data.mapProperties.end(); ++it) {
        String key                = it->first;
        GuiConstant::constant_t c = GuiConstant::getConstantByName(key);

        //dbgassert(c.idx > 0);
        // some constants may not be defined, and thats ok
        if (c.idx > 0) {
            out.mapProperties[c.idx] = it->second;
        }
    }
    for (auto it = data.mapFonts.begin(); it != data.mapFonts.end(); ++it) {
        String key            = it->first;
        UIFont::font_type_t c = UIFont::getConstantByName(key);
        // some constants may not be defined, and thats ok
        if (c.idx > 0) {
            out.mapFonts[c.idx] = UIFont::font_instance{it->second};
        }
    }
}

template <class Archive>
void serialize(Archive& archive, NVGcolor& m) {
    archive(make_nvp("r", m.r), make_nvp("g", m.g), make_nvp("b", m.b), make_nvp("a", m.a));
}

template <class Archive>
void serialize(Archive& archive, theme_data& m) {
    archive(make_nvp("colors", m.mapColors));
    archive(make_nvp("properties", m.mapProperties));
    archive(make_nvp("fonts", m.mapFonts));
}
template <class Archive>
void save(Archive& archive, guitheme_t const& m) {
    archive(make_nvp("name", m.name));

    // convert theme data to a data structure that is easier to serialize
    theme_data data;
    storeThemeData(m, data);
    archive(make_nvp("data", data));
}

template <class Archive>
void load(Archive& archive, guitheme_t& m) {
    m.mapColors.clear();
    m.mapProperties.clear();
    m.mapFonts.clear();
    m.initTheme();
    m.mapColors.clear();
    m.mapProperties.clear();
    archive(make_nvp("name", m.name));

    theme_data data;
    archive(make_nvp("data", data));
    loadThemeData(data, m);
}
template <class Archive>
void serialize(Archive& archive, themefile& m) {
    archive(make_nvp("default", m.defaultTheme), make_nvp("current", m.theme), make_nvp("themes", m.themes));
}
themefile loadThemeFile() {
    Stringstream ss;

    String cwdPathTheme = toUserdataPath(THEMEFILE_NAME);
    ifstream file(cwdPathTheme, ifstream::in);
    if (file) {
        ss << file.rdbuf();
        std::streampos length = file.tellg();
        if (length > 10) {
            themefile tmpSettings;
            cereal::JSONInputArchive ar(ss);
            ar(tmpSettings);
            return tmpSettings;
        }
    }
    throw std::runtime_error("Failed reading theme file " + cwdPathTheme);
}
void saveThemeFile(themefile& _settings) {
    String cwdPathTheme = toUserdataPath(THEMEFILE_NAME);
    ofstream file;
    file.exceptions(~ofstream::goodbit);
    file.open(cwdPathTheme, ofstream::out);
    cereal::JSONOutputArchive ar(file);
    ar(_settings);
}
