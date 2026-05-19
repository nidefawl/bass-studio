#include "appsettingsfile-v2.hpp"
#include "jsonfile.hpp"
#include "appsettings.hpp"
#include "fileio.hpp"
#include "platform.hpp"
#include "logging.hpp"
#include "seq_datetime.hpp"

#include <cereal/external/base64.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace nlohmann::literals;

#define JSON_FROM_TO NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT

// ============================================================================
// JSON Serializers for GLM Types
// ============================================================================

namespace glm {
JSON_FROM_TO(ivec2, x, y)
JSON_FROM_TO(ivec3, x, y, z)
JSON_FROM_TO(ivec4, x, y, z, w)
JSON_FROM_TO(vec2, x, y)
JSON_FROM_TO(vec3, x, y, z)
JSON_FROM_TO(vec4, x, y, z, w)
} // namespace glm

// ============================================================================
// JSON Serializers for AudioIO Types
// ============================================================================

namespace DAW::AudioIO {
JSON_FROM_TO(io_cfg_channel, idx, offset, name, type)
JSON_FROM_TO(io_cfg_tracks, isInit, input, output)
} // namespace DAW::AudioIO

// ============================================================================
// JSON Serializers for AppSettings Components
// ============================================================================

// io_channel, midi_channel, audio/MIDI configs
JSON_FROM_TO(io_channel, idx, channels)
JSON_FROM_TO(midi_channel, idx, deviceName, channels)
JSON_FROM_TO(app_ioaudioconfig, device_api, deviceNameInput, deviceNameOutput)
JSON_FROM_TO(app_ioasioconfig, device_api, deviceName, inputs, outputs)
JSON_FROM_TO(app_iomidiconfig, device_api, inputs, outputs)

// Main io_settings struct
JSON_FROM_TO(app_iosettings, samplerate, blocksize, internalSamplerate, internalBlocksize, 
             device_api, asioConfig, midiconfigs, configs, channelConfigs)

// Plugin configuration
JSON_FROM_TO(app_vst2_config, uidRemapping)
JSON_FROM_TO(app_plugin_configuration, pathClap, pathVst2, pathVst3, pathLv2, configVst2)

// DAW settings, autosave, path remapping
JSON_FROM_TO(app_daw_settings, startupProjectPath, startupLoadDeffered, audioEnabled, vmmode,
             debugMode, shaderDebug, uiLayoutLocked, uiShowSettingsClip, uiShowSettingsArp, lowLatencyMode)
JSON_FROM_TO(app_autosave_settings, tmSaveDelayMinutes, tmReminderDelayMinutes)
JSON_FROM_TO(app_path_remapping, pathRemapping)

// Grid and window settings
JSON_FROM_TO(grid_density, enabled, isfixed, fixedBars, dynamicDensity, triplets)
JSON_FROM_TO(appwindowsettings, size, dens, zoom, flags)

// recentfilelist
JSON_FROM_TO(recentfilelist, sortedEntries, recentFilesMeta)

// ============================================================================
// Custom JSON Serializers for Special Types
// ============================================================================

// appwindow_size_t (binary data as base64)
inline void to_json(json& j, const appwindow_size_t& m) {
    auto base64string = cereal::base64::encode(reinterpret_cast<const unsigned char*>(m.data), sizeof(m.data));
    j = json{
        {"data", base64string},
        {"type", m.type},
        {"valid", m.valid}
    };
}

inline void from_json(const json& j, appwindow_size_t& m) {
    j.at("type").get_to(m.type);
    j.at("valid").get_to(m.valid);
    auto base64string = j.at("data").get<std::string>();
    auto decoded = cereal::base64::decode(base64string);
    if (decoded.size() != sizeof(m.data)) {
        throw std::runtime_error("Invalid appwindow_size_t data size");
    }
    std::memcpy(m.data, decoded.data(), sizeof(m.data));
}

// recentfilelistentry (with new timestamp field)
inline void to_json(json& j, const recentfilelistentry& entry) {
    j = json{
        {"path", entry.path},
        {"timestamp", entry.timestamp}
    };
    if (!entry.isoDate.empty()) {
        j["isoDate"] = entry.isoDate;
    }
}

inline void from_json(const json& j, recentfilelistentry& entry) {
    entry.path = j.at("path").get<String>();
    
    // Support both old string "date" field and new int64_t timestamp
    if (j.contains("timestamp")) {
        entry.timestamp = j.at("timestamp").get<int64_t>();
    } else if (j.contains("date")) {
        // Fallback: old string date ignored, timestamp defaults to 0
        entry.timestamp = 0;
    }
    
    if (j.contains("isoDate")) {
        entry.isoDate = j.at("isoDate").get<String>();
    } else {
        entry.isoDate = GetInt64DateAsLocalizedTimeStr(entry.timestamp);
    }
}

namespace DAW::AppSettingsV2 {

// ============================================================================
// Helper Functions (using DAW::JsonFileIO utilities)
// ============================================================================

// Note: readJsonFromFile and writeJsonToFile are now provided by jsonfile.hpp
// This namespace uses them via DAW::JsonFileIO::

// ============================================================================
// Individual File Loaders
// ============================================================================

namespace Internal {

std::optional<std::string> loadDawSettings(app_daw_settings& out, app_autosave_settings& autosave,
                                           app_path_remapping& paths, std::vector<appwindowsettings>& windows,
                                           std::vector<String>& userLibraries, String& theme, bool& saveOnExit) {
    String filePath = App::Platform::toUserdataPath("data/app-settings.json");
    auto optJson = DAW::JsonFileIO::readJsonFromFile(filePath);
    
    if (!optJson.has_value()) {
        return std::nullopt; // File doesn't exist or couldn't be read; use defaults
    }
    
    try {
        const auto& j = optJson.value();
        
        if (j.contains("dawsettings")) {
            out = j.at("dawsettings").get<app_daw_settings>();
        }
        if (j.contains("autosave")) {
            autosave = j.at("autosave").get<app_autosave_settings>();
        }
        if (j.contains("pathmapping")) {
            paths = j.at("pathmapping").get<app_path_remapping>();
        }
        if (j.contains("windowSettings")) {
            windows = j.at("windowSettings").get<std::vector<appwindowsettings>>();
        }
        if (j.contains("userlibraries")) {
            userLibraries = j.at("userlibraries").get<std::vector<String>>();
        }
        if (j.contains("theme")) {
            theme = j.at("theme").get<String>();
        }
        if (j.contains("saveOnExit")) {
            saveOnExit = j.at("saveOnExit").get<bool>();
        }
        
        return std::nullopt; // success
    } catch (const std::exception& e) {
        return std::string("Failed to load daw settings: ") + e.what();
    }
}

std::optional<std::string> loadAudioSettings(app_iosettings& out) {
    String filePath = App::Platform::toUserdataPath("data/audio-settings.json");
    auto optJson = DAW::JsonFileIO::readJsonFromFile(filePath);
    
    if (!optJson.has_value()) {
        return std::nullopt;
    }
    
    try {
        out = optJson.value().get<app_iosettings>();
        return std::nullopt; // success
    } catch (const std::exception& e) {
        return std::string("Failed to load audio settings: ") + e.what();
    }
}

std::optional<std::string> loadPluginSettings(app_plugin_configuration& out) {
    String filePath = App::Platform::toUserdataPath("data/plugin-settings.json");
    auto optJson = DAW::JsonFileIO::readJsonFromFile(filePath);
    
    if (!optJson.has_value()) {
        return std::nullopt;
    }
    
    try {
        out = optJson.value().get<app_plugin_configuration>();
        return std::nullopt; // success
    } catch (const std::exception& e) {
        return std::string("Failed to load plugin settings: ") + e.what();
    }
}

std::optional<std::string> loadRecentFiles(recentfilelist& out) {
    String filePath = App::Platform::toUserdataPath("data/recent-files.json");
    auto optJson = DAW::JsonFileIO::readJsonFromFile(filePath);
    
    if (!optJson.has_value()) {
        return std::nullopt;
    }
    
    try {
        out = optJson.value().get<recentfilelist>();
        return std::nullopt; // success
    } catch (const std::exception& e) {
        return std::string("Failed to load recent files: ") + e.what();
    }
}

} // namespace Internal

// ============================================================================
// Individual File Savers
// ============================================================================

namespace Internal {

std::optional<std::string> saveDawSettings(const app_daw_settings& in, const app_autosave_settings& autosave,
                                           const app_path_remapping& paths, const std::vector<appwindowsettings>& windows,
                                           const std::vector<String>& userLibraries, const String& theme, bool saveOnExit) {
    try {
        json j;
        j["version"] = 2;
        j["dawsettings"] = in;
        j["autosave"] = autosave;
        j["pathmapping"] = paths;
        j["windowSettings"] = windows;
        j["userlibraries"] = userLibraries;
        j["theme"] = theme;
        j["saveOnExit"] = saveOnExit;
        
        String filePath = App::Platform::toUserdataPath("data/app-settings.json");
        return DAW::JsonFileIO::writeJsonToFile(j, filePath, 2);
    } catch (const std::exception& e) {
        return std::string("Failed to save daw settings: ") + e.what();
    }
}

std::optional<std::string> saveAudioSettings(const app_iosettings& in) {
    try {
        json j;
        j["version"] = 2;
        j = in; // nlohmann auto-conversion
        
        String filePath = App::Platform::toUserdataPath("data/audio-settings.json");
        return DAW::JsonFileIO::writeJsonToFile(j, filePath, 2);
    } catch (const std::exception& e) {
        return std::string("Failed to save audio settings: ") + e.what();
    }
}

std::optional<std::string> savePluginSettings(const app_plugin_configuration& in) {
    try {
        json j;
        j["version"] = 2;
        j = in;
        
        String filePath = App::Platform::toUserdataPath("data/plugin-settings.json");
        return DAW::JsonFileIO::writeJsonToFile(j, filePath, 2);
    } catch (const std::exception& e) {
        return std::string("Failed to save plugin settings: ") + e.what();
    }
}

std::optional<std::string> saveRecentFiles(const recentfilelist& in) {
    try {
        json j;
        j["version"] = 2;
        j = in;
        
        String filePath = App::Platform::toUserdataPath("data/recent-files.json");
        return DAW::JsonFileIO::writeJsonToFile(j, filePath, 2);
    } catch (const std::exception& e) {
        return std::string("Failed to save recent files: ") + e.what();
    }
}

} // namespace Internal

// ============================================================================
// Public API: Orchestrator Functions
// ============================================================================

void loadSettings(appsettings& out) {
    // Load all 4 config files, using defaults if any are missing
    auto errDaw = Internal::loadDawSettings(out.dawsettings, out.autosave, out.pathmapping,
                                            out.windowSettings, out.userLibraryPaths, 
                                            out.selectedTheme, out.saveOnExit);
    if (errDaw) {
        log_lf(Log::L_WARN, "Error loading daw settings: %s\n", errDaw.value().c_str());
    }
    
    auto errAudio = Internal::loadAudioSettings(out.iosettings);
    if (errAudio) {
        log_lf(Log::L_WARN, "Error loading audio settings: %s\n", errAudio.value().c_str());
    }
    
    auto errPlugin = Internal::loadPluginSettings(out.pluginsettings);
    if (errPlugin) {
        log_lf(Log::L_WARN, "Error loading plugin settings: %s\n", errPlugin.value().c_str());
    }
    
    auto errRecent = Internal::loadRecentFiles(out.recentfiles);
    if (errRecent) {
        log_lf(Log::L_WARN, "Error loading recent files: %s\n", errRecent.value().c_str());
    }
    
    // On first startup, ensure all default files are created
    // Check if any config file is missing - if so, write all defaults
    bool hasAppSettings = FileExists(App::Platform::toUserdataPath("data/app-settings.json"));
    bool hasAudioSettings = FileExists(App::Platform::toUserdataPath("data/audio-settings.json"));
    bool hasPluginSettings = FileExists(App::Platform::toUserdataPath("data/plugin-settings.json"));
    // recent-files.json is optional, so not checked here
    
    if (!hasAppSettings || !hasAudioSettings || !hasPluginSettings) {
        log_lf(Log::L_WARN, "Creating missing settings files with defaults...\n");
        saveSettings(out);
    }
    
    out.fileFmtVersion = 2;
}

void saveSettings(appsettings& _settings) {
    auto errDaw = Internal::saveDawSettings(_settings.dawsettings, _settings.autosave, _settings.pathmapping,
                                            _settings.windowSettings, _settings.userLibraryPaths, 
                                            _settings.selectedTheme, _settings.saveOnExit);
    if (errDaw) {
        log_lf(Log::L_WARN, "Error saving daw settings: %s\n", errDaw.value().c_str());
    }
    
    auto errAudio = Internal::saveAudioSettings(_settings.iosettings);
    if (errAudio) {
        log_lf(Log::L_WARN, "Error saving audio settings: %s\n", errAudio.value().c_str());
    }
    
    auto errPlugin = Internal::savePluginSettings(_settings.pluginsettings);
    if (errPlugin) {
        log_lf(Log::L_WARN, "Error saving plugin settings: %s\n", errPlugin.value().c_str());
    }
    
    auto errRecent = Internal::saveRecentFiles(_settings.recentfiles);
    if (errRecent) {
        log_lf(Log::L_WARN, "Error saving recent files: %s\n", errRecent.value().c_str());
    }
}

} // namespace DAW::AppSettingsV2

// ============================================================================
// Global API Functions (C-linkage compatible, for backward compatibility)
// ============================================================================

void loadSettings(appsettings& settings) {
    DAW::AppSettingsV2::loadSettings(settings);
}

void saveSettings(appsettings& _settings) {
    DAW::AppSettingsV2::saveSettings(_settings);
}
