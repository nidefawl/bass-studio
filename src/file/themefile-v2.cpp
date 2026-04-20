#include "themefile.hpp"
#include "jsonfile.hpp"
#include "fileio.hpp"
#include "logging.hpp"
#include "str_util.hpp"
#include "color_util.hpp"
#include "config.hpp"
#include "theme.hpp"
#include "exceptions.hpp"
#include "guiconstant.hpp"
#include "guicolors.hpp"
#include "msgbox.hpp"
#include "renderresources.hpp"
#include "assert_dbg.h"
#include "platform.hpp"
#include "math/seq_math.hpp"

#include <cmath>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>
#include <optional>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace nlohmann::literals;

#define JSON_FROM_TO NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT

// ============================================================================
// JSON Serializers for GLM Types
// ============================================================================

namespace glm {
JSON_FROM_TO(vec2, x, y)
} // namespace glm

// ============================================================================
// JSON Serializers for NanoVG Color and Background Image
// ============================================================================

inline void to_json(json& j, const NVGcolor& m) {
    j = json{
        {"r", m.r},
        {"g", m.g},
        {"b", m.b},
        {"a", m.a}
    };
}

inline void from_json(const json& j, NVGcolor& m) {
    m.r = j.at("r").get<float>();
    m.g = j.at("g").get<float>();
    m.b = j.at("b").get<float>();
    m.a = j.at("a").get<float>();
}

inline void to_json(json& j, const container_background_image& m) {
    j = json{
        {"path", m.path},
        {"layout", (int32_t)m.layout},
        {"verticalPos", (int32_t)m.verticalPos},
        {"horizontalPos", (int32_t)m.horizontalPos},
        {"scale", m.scale},
        {"scaleAbsolute", m.scaleAbsolute},
        {"rgba", m.rgba}
    };
}

inline void from_json(const json& j, container_background_image& m) {
    m.path = j.at("path").get<String>();
    m.layout = (container_background_image::layout_t)j.at("layout").get<int32_t>();
    m.verticalPos = (container_background_image::position_t)j.at("verticalPos").get<int32_t>();
    m.horizontalPos = (container_background_image::position_t)j.at("horizontalPos").get<int32_t>();
    m.scale = j.at("scale").get<glm::vec2>();
    m.scaleAbsolute = j.at("scaleAbsolute").get<bool>();
    m.rgba = j.at("rgba").get<uint32_t>();
}

// ============================================================================
// Theme Data (intermediary structure for serialization)
// ============================================================================

struct theme_data {
    std::unordered_map<String, uint32_t> mapColors;
    std::unordered_map<String, int32_t> mapProperties;
    std::unordered_map<String, String> mapFonts;
    std::unordered_map<String, container_background_image> mapBackgrounds;
};

inline void to_json(json& j, const theme_data& m) {
    j = json{
        {"colors", m.mapColors},
        {"properties", m.mapProperties},
        {"fonts", m.mapFonts},
        {"images", m.mapBackgrounds}
    };
}

inline void from_json(const json& j, theme_data& m) {
    if (j.contains("colors")) {
        m.mapColors = j.at("colors").get<std::unordered_map<String, uint32_t>>();
    }
    if (j.contains("properties")) {
        m.mapProperties = j.at("properties").get<std::unordered_map<String, int32_t>>();
    }
    if (j.contains("fonts")) {
        m.mapFonts = j.at("fonts").get<std::unordered_map<String, String>>();
    }
    if (j.contains("images")) {
        m.mapBackgrounds = j.at("images").get<std::unordered_map<String, container_background_image>>();
    }
}

namespace DAW::ThemeFile::ThemeFormatV2 {

// ============================================================================
// Theme Data Conversion Functions
// ============================================================================

/**
 * Convert internal guitheme_t structure to JSON-serializable theme_data.
 * Maps numeric IDs to symbolic names for readability and forward compatibility.
 */
static void storeThemeData(const guitheme_t& m, theme_data& out) {
    // Colors: map numeric ID to color name
    for (const auto& mapColor : m.mapColors) {
        auto c = GuiColor::getConstantById(mapColor.first);
        if (c.idx == 0) continue;
        out.mapColors[c.name] = mapColor.second;
    }
    
    // Properties: map numeric ID to property name
    for (const auto& mapProperty : m.mapProperties) {
        GuiConstant::constant_t c = GuiConstant::getConstantById(mapProperty.first);
        if (c.idx == 0) continue;
        out.mapProperties[c.name] = mapProperty.second;
    }
    
    // Fonts: map numeric ID to font type name
    for (const auto& mapFont : m.mapFonts) {
        UIFont::font_type_t c = UIFont::getConstantById(mapFont.first);
        if (c.idx == 0) continue;
        out.mapFonts[c.name] = mapFont.second.name;
    }
    
    // Background images: map numeric ID to background type name
    for (const auto& mapBackground : m.mapBackgrounds) {
        GuiBackgroundImage::constant_t c = GuiBackgroundImage::getConstantById(mapBackground.first);
        if (c.idx == 0) continue;
        out.mapBackgrounds[c.name] = mapBackground.second;
    }
}

/**
 * Convert JSON-serialized theme_data to internal guitheme_t structure.
 * Maps symbolic names back to numeric IDs, with graceful degradation for unknown constants.
 */
static void loadThemeData(const theme_data& data, guitheme_t& out) {
    // Colors: map color name to numeric ID and update NVG color cache
    for (const auto& mapColor : data.mapColors) {
        auto c = GuiColor::getConstantByName(mapColor.first);
        if (c.idx == 0) {
            log_lf(Log::L_WARN, "Unknown color constant: %s\n", StringAsCStr(mapColor.first));
            continue;
        }
        out.mapColors[c.idx] = mapColor.second;
        
        if (c.idx < out.vecNVGColors.size()) {
            out.vecNVGColors[c.idx] = rgbaToNvg(mapColor.second);
        } else {
            log_lf(Log::L_WARN, "Color index %u out of range for NVG cache\n", c.idx);
        }
    }
    
    // Properties: map property name to numeric ID with clamping to valid range
    for (const auto& mapProperty : data.mapProperties) {
        GuiConstant::constant_t c = GuiConstant::getConstantByName(mapProperty.first);
        if (c.idx == 0) {
            log_lf(Log::L_WARN, "Unknown property constant: %s\n", StringAsCStr(mapProperty.first));
            continue;
        }
        
        // Clamp property to valid range
        int32_t clampedValue = math::clamp<int32_t>(mapProperty.second, c.rangeMin, c.rangeMax);
        if (clampedValue != mapProperty.second) {
            log_lf(Log::L_WARN, "Property %s clamped from %d to %d\n", 
                   StringAsCStr(mapProperty.first), mapProperty.second, clampedValue);
        }
        
        out.mapProperties[c.idx] = clampedValue;
    }
    
    // Fonts: map font name to numeric ID
    for (const auto& mapFont : data.mapFonts) {
        UIFont::font_type_t c = UIFont::getConstantByName(mapFont.first);
        if (c.idx == 0) {
            log_lf(Log::L_WARN, "Unknown font constant: %s\n", StringAsCStr(mapFont.first));
            continue;
        }
        
        out.mapFonts[c.idx] = UIFont::font_instance{mapFont.second};
    }
    
    // Background images: map background type name to numeric ID
    for (const auto& mapBackground : data.mapBackgrounds) {
        GuiBackgroundImage::constant_t c = GuiBackgroundImage::getConstantByName(mapBackground.first);
        if (c.idx == 0) {
            log_lf(Log::L_WARN, "Unknown background image constant: %s\n", StringAsCStr(mapBackground.first));
            continue;
        }
        
        out.mapBackgrounds[c.idx] = mapBackground.second;
    }
}

// ============================================================================
// Helper Functions (using DAW::JsonFileIO utilities)
// ============================================================================

// Note: readJsonFromFile and writeJsonToFile are now provided by jsonfile.hpp
// This namespace uses them via DAW::JsonFileIO::

// ============================================================================
// Public API
// ============================================================================

/**
 * Load a theme from the specified directory path.
 * Expects a file named THEMEFILE_NAME (typically "theme.json").
 * 
 * @param path Directory containing the theme file
 * @return Populated themefile structure
 * @throws FileIOException if the theme cannot be loaded
 */
ThemeFile::themefile loadTheme(const String& path) {
    String pathFile = path + "/" + THEMEFILE_NAME;
    App::Platform::sanitizePathToFile(pathFile);
    
    auto optJson = DAW::JsonFileIO::readJsonFromFile(pathFile);
    
    if (!optJson.has_value()) {
        throw FileIOException("Failed reading theme file " + pathFile);
    }
    
    try {
        themefile result;
        result.theme = guitheme_t();
        
        const auto& j = optJson.value();
        
        // Extract theme name
        if (j.contains("name")) {
            result.theme.name = j.at("name").get<String>();
        }
        
        // Extract theme data and convert to internal format
        if (j.contains("data")) {
            theme_data data = j.at("data").get<theme_data>();
            loadThemeData(data, result.theme);
        }
        
        result.theme.version = 2;
        return result;
        
    } catch (const std::exception& e) {
        throw FileIOException(String("Failed to load theme file ") + pathFile + ": " + e.what());
    }
}

/**
 * Save a theme to the specified directory path.
 * Writes a file named THEMEFILE_NAME (typically "theme.json") with pretty-printed JSON.
 * 
 * @param path Directory where the theme will be saved
 * @param theme Theme structure to save
 * @return true if successful, false otherwise
 */
bool saveTheme(const String& path, ThemeFile::themefile& _settings) {
    try {
        String pathSanitized = path;
        App::Platform::sanitizePathToDirectory(pathSanitized);
        CreateDirectoryIfNotExists(pathSanitized);
        
        String pathFile = pathSanitized + "/" + THEMEFILE_NAME;
        
        // Convert internal theme structure to JSON-serializable format
        json j;
        j["version"] = 2;
        j["name"] = _settings.theme.name;
        
        theme_data data;
        storeThemeData(_settings.theme, data);
        j["data"] = data;
        
        auto optError = DAW::JsonFileIO::writeJsonToFile(j, pathFile, 2);
        
        if (optError.has_value()) {
            log_lf(Log::L_ERROR, "saveTheme error: %s\n", StringAsCStr(optError.value()));
            return false;
        }
        
        return true;
        
    } catch (const FileIOException& e) {
        log_lf(Log::L_ERROR, "saveTheme File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_lf(Log::L_ERROR, "saveTheme exception: %s\n", e.what());
    }
    
    return false;
}

} // namespace DAW::ThemeFile::ThemeFormatV2
