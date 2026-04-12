#pragma once

#include "appsettings.hpp"
#include <string>
#include <optional>

namespace DAW::AppSettingsV2 {

/**
 * @brief Load application settings from v2 split JSON config files
 * Loads settings from:
 * - app-settings.json (DAW settings, autosave, windows, paths, theme)
 * - audio-settings.json (audio device configuration)
 * - plugin-settings.json (VST/CLAP paths, plugin config)
 * - recent-files.json (recent project list)
 * 
 * Missing files use struct defaults. Called automatically by public loadSettings().
 */
void loadSettings(appsettings& out);

/**
 * @brief Save application settings to v2 split JSON config files
 * Writes all 4 JSON config files with atomic writes (temp+rename).
 * Called automatically by public saveSettings().
 */
void saveSettings(const appsettings& in);

// Individual file operations (for granular updates if needed)
namespace Internal {
    std::optional<std::string> loadDawSettings(app_daw_settings& out, app_autosave_settings& autosave, 
                                               app_path_remapping& paths, std::vector<appwindowsettings>& windows,
                                               std::vector<String>& userLibraries, String& theme, bool& saveOnExit);
    
    std::optional<std::string> loadAudioSettings(app_iosettings& out);
    std::optional<std::string> loadPluginSettings(app_plugin_configuration& out);
    std::optional<std::string> loadRecentFiles(recentfilelist& out);
    
    std::optional<std::string> saveDawSettings(const app_daw_settings& in, const app_autosave_settings& autosave,
                                               const app_path_remapping& paths, const std::vector<appwindowsettings>& windows,
                                               const std::vector<String>& userLibraries, const String& theme, bool saveOnExit);
    
    std::optional<std::string> saveAudioSettings(const app_iosettings& in);
    std::optional<std::string> savePluginSettings(const app_plugin_configuration& in);
    std::optional<std::string> saveRecentFiles(const recentfilelist& in);
}

} // namespace DAW::AppSettingsV2
