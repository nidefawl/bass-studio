#pragma once
#include <vector>
#include "config.hpp"
#include "theme.hpp"

namespace DAW::ThemeFile { 

struct themefile {
public:
    guitheme_t theme;
    themefile() = default;
};

namespace ThemeFormatV1 { 
themefile loadTheme(const String& path);
bool saveTheme(const String& path, themefile& _settings);
} // namespace ThemeFormatV1

namespace ThemeFormatV2 {

/**
 * @brief Load a theme from the specified directory path.
 * Expects a file named THEMEFILE_NAME (typically "theme.json") in the directory.
 * 
 * Uses nlohmann JSON format with named color/property/font/background constants
 * for forward compatibility and readability.
 * 
 * @param path Directory containing the theme file
 * @return Populated themefile structure with version set to 2
 * @throws FileIOException if the theme cannot be loaded or parsed
 */
themefile loadTheme(const String& path);

/**
 * @brief Save a theme to the specified directory path.
 * Writes a file named THEMEFILE_NAME (typically "theme.json") with pretty-printed JSON.
 * 
 * Converts internal numeric ID mappings to symbolic names for human readability
 * and forward compatibility with future theme constant changes.
 * 
 * @param path Directory where the theme will be saved (created if necessary)
 * @param theme Theme structure to save
 * @return true if successful, false otherwise
 */
bool saveTheme(const String& path, themefile& _theme);

} // namespace ThemeFormatV2
} // namespace DAW::ThemeFile

