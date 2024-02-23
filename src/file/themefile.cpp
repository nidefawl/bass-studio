#include "themefile.h"
#include "fileio.h"
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
#include <cereal_optional_nvp/cereal_optional_nvp.hpp>

using namespace cereal;

struct theme_data {
    std::unordered_map<String, uint32_t> mapColors;
    std::unordered_map<String, int32_t> mapProperties;
    std::unordered_map<String, String> mapFonts;
    std::unordered_map<String, container_background_image> mapBackgrounds;
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
    for (const auto& mapFont : m.mapFonts) {
        UIFont::font_type_t c = UIFont::getConstantById(mapFont.first);
        if (c.idx == 0) continue;
        out.mapFonts[c.name] = mapFont.second.name;
    }
    for (const auto& mapBackground : m.mapBackgrounds) {
        GuiBackgroundImage::constant_t c = GuiBackgroundImage::getConstantById(mapBackground.first);
        if (c.idx == 0) continue;
        out.mapBackgrounds[c.name] = mapBackground.second;
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
    for (const auto& mapBackground : data.mapBackgrounds) {
        GuiBackgroundImage::constant_t c = GuiBackgroundImage::getConstantByName(mapBackground.first);
        // some constants may not be defined, and thats ok
        if (c.idx > 0) {
            out.mapBackgrounds[c.idx] = mapBackground.second;
        }
    }
}

template <class Archive>
void serialize(Archive& archive, NVGcolor& m) {
    archive(make_nvp("r", m.r), make_nvp("g", m.g), make_nvp("b", m.b), make_nvp("a", m.a));
}

namespace glm
{
template<class Archive>
void serialize(Archive& archive, glm::vec2& m) {
    archive(make_nvp("x", m.x), make_nvp("y", m.y));
}
} // namespace glm

template <class Archive>
void serialize(Archive& archive, container_background_image& m) {
    archive(make_nvp("path", m.path),
            make_nvp("layout", m.layout),
            make_nvp("verticalPos", m.verticalPos),
            make_nvp("horizontalPos", m.horizontalPos),
            make_nvp("scale", m.scale),
            make_nvp("scaleAbsolute", m.scaleAbsolute));
    make_optional_nvp(archive, "rgba", m.rgba);
}

template <class Archive>
void serialize(Archive& archive, theme_data& m) {
    archive(make_nvp("colors", m.mapColors));
    archive(make_nvp("properties", m.mapProperties));
    archive(make_nvp("fonts", m.mapFonts));
    archive(make_nvp("images", m.mapBackgrounds));
}
template <class Archive>
void save(Archive& archive, themefile const& m) {
    archive(make_nvp("name", m.theme.name));

    // convert theme data to a data structure that is easier to serialize
    theme_data data;
    storeThemeData(m.theme, data);
    archive(make_nvp("data", data));
}

template <class Archive>
void load(Archive& archive, themefile& m) {
    m.theme = guitheme_t();
    archive(make_nvp("name", m.theme.name));
    theme_data data;
    archive(make_nvp("data", data));
    loadThemeData(data, m.theme);
}

themefile loadTheme(const String& path) {
    Stringstream ss;
    String pathFile = path + "/" + THEMEFILE_NAME;
    App::Platform::sanitizePathToFile(pathFile);
    std::ifstream file(pathFile, std::ifstream::in);
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
    throw FileIOException("Failed reading theme file " + pathFile);
}
void saveTheme(const String& path, themefile& _settings) {
    CreateDirectoryIfNotExists(path);
    String pathFile = path + "/" + THEMEFILE_NAME;
    App::Platform::sanitizePathToFile(pathFile);
    std::ofstream file;
    file.exceptions(~std::ofstream::goodbit);
    file.open(pathFile, std::ofstream::out);
    cereal::JSONOutputArchive ar(file);
    ar(_settings);
}
