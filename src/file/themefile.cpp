#include "themefile.h"
#include "str_util.h"
#include "color_util.h"
#include "config.h"
#include "theme.h"
#include "exceptions.h"
#include "guiconstant.h"
#include "guicolors.h"
#include "msgbox.h"
#include "renderresources.h"
#include "assert_dbg.h"
#include "platform.h"
#include "math/seq_math.h"

#include <cmath>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>

using namespace cereal;

struct theme_data {
    std::unordered_map<String, uint32_t> mapColors;
    std::unordered_map<String, int32_t> mapProperties;
    std::unordered_map<String, String> mapFonts;
};

void storeThemeData(guitheme_t const& m, theme_data& out) {
    for (const auto& mapColor : m.mapColors) {
        auto c = GuiColor::getConstantById(mapColor.first);
        if (c.idx == 0) continue;
        out.mapColors[c.name] = mapColor.second;
    }
    for (const auto& mapPropertie : m.mapProperties) {
        GuiConstant::constant_t c = GuiConstant::getConstantById(mapPropertie.first);
        if (c.idx == 0) continue;
        out.mapProperties[c.name] = mapPropertie.second;
    }
    for (const auto & mapFont : m.mapFonts) {
        UIFont::font_type_t c = UIFont::getConstantById(mapFont.first);
        if (c.idx == 0) continue;
        out.mapFonts[c.name] = mapFont.second.name;
    }
}
void loadThemeData(theme_data& data, guitheme_t& out) {
    for (const auto& mapColor : data.mapColors) {
        auto c = GuiColor::getConstantByName(mapColor.first);
        if (c.idx == 0) continue;
        out.mapColors[c.idx] = mapColor.second;
        dbgassert(c.idx < out.vecNVGColors.size());
        out.vecNVGColors[c.idx] = rgbaToNvg(mapColor.second);
    }
    for (const auto& mapPropertie : data.mapProperties) {
        GuiConstant::constant_t c = GuiConstant::getConstantByName(mapPropertie.first);
        //dbgassert(c.idx == 0);
        // some constants may not be defined, and thats ok
        if (c.idx > 0) {
            out.mapProperties[c.idx] = math::clamp<int32_t>(mapPropertie.second, c.rangeMin, c.rangeMax);
        }
    }
    for (const auto& mapFont : data.mapFonts) {
        UIFont::font_type_t c = UIFont::getConstantByName(mapFont.first);
        // some constants may not be defined, and thats ok
        if (c.idx > 0) {
            out.mapFonts[c.idx] = UIFont::font_instance{mapFont.second};
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
    m = guitheme_t();
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

    String cwdPathTheme = App::Platform::toUserdataPath(THEMEFILE_NAME);
    std::ifstream file(cwdPathTheme, std::ifstream::in);
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
    throw appexception("Failed reading theme file " + cwdPathTheme);
}
void saveThemeFile(themefile& _settings) {
    String cwdPathTheme = App::Platform::toUserdataPath(THEMEFILE_NAME);
    std::ofstream file;
    file.exceptions(~std::ofstream::goodbit);
    file.open(cwdPathTheme, std::ofstream::out);
    cereal::JSONOutputArchive ar(file);
    ar(_settings);
}
