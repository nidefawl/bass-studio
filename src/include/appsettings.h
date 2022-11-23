#pragma once
#include "platform.h"
#include "types.h"
#include "config.h"
#include "grid.h"
#include "host/audio_config.h"
#include "str_util.h"
#include <vector>
#include <memory>
#include <map>

struct appwindow_size_t;

struct io_channel {
    int32_t idx;
    std::vector<channelnum_t> channels;
};

struct midi_channel {
    int32_t idx;
    String deviceName;
    std::vector<channelnum_t> channels;
};

struct app_ioaudioconfig {
    String device_api;
    String deviceNameInput = "default";
    String deviceNameOutput = "default";
};

struct app_ioasioconfig {
    String device_api;
    String deviceName;
    std::vector<io_channel> outputs;
    std::vector<io_channel> inputs;
};

struct app_iomidiconfig {
    String device_api;
    std::vector<midi_channel> outputs;
    std::vector<midi_channel> inputs;
};

struct app_iosettings {
    samplerate_t samplerate = 44100;
    blocksize_t blocksize = 512;
    samplerate_t internalSamplerate = 44100;
    blocksize_t internalBlocksize = 512;
    app_ioasioconfig asioConfig;
    std::map<String, app_ioaudioconfig> configs;
    std::map<String, app_iomidiconfig> midiconfigs;
    std::map<String, DAW::AudioIO::io_cfg_tracks> channelConfigs;
    String device_api;
    app_ioasioconfig& getAsioConfig() { return asioConfig; }
    DAW::AudioIO::io_cfg_tracks& getChannelConfig(const String& devApi) {
        if (!channelConfigs.count(devApi)) {
            channelConfigs[devApi] = DAW::AudioIO::io_cfg_tracks();
        }
        return channelConfigs[devApi];
    }
    app_ioaudioconfig& getConfig(const String& devApi) {
        if (!configs.count(devApi)) {
            configs[devApi] = app_ioaudioconfig();
        }
        return configs[devApi];
    }
    app_iomidiconfig& getIOConfigMidi(const String& devApi) {
        if (!midiconfigs.count(devApi)) {
            midiconfigs[devApi] = app_iomidiconfig();
        }
        return midiconfigs[devApi];
    }
};

struct appwindowsettings {
    appwindow_size_t size{};
    grid_density dens{};
    float zoom = 1.0f;
    uint32_t flags = 0;
};

struct recentfilelistentry {
    String path;
    String date;
};

class recentfilelist {
public:
    std::vector<String> sortedEntries;
    std::map<String, recentfilelistentry> recentFilesMeta;
    void add(const String& path);
};

struct app_vst2_config {
    std::map<uint32_t, uint32_t> uidRemapping;
};

struct app_plugin_configuration {
    String pathClap = DAW_PLATFORM_CLAP_PATH_DEFAULT;
    String pathVst2 = DAW_PLATFORM_VST2_PATH_DEFAULT;
    app_vst2_config configVst2;
};

struct app_path_remapping {
    std::map<String, String> pathRemapping;
};
struct app_autosave_settings {
    int64_t tmSaveDelayMinutes     = 60L;
    int64_t tmReminderDelayMinutes = 5L;
};
struct app_daw_settings {
    String startupProjectPath;
    bool startupLoadDeffered = true;
    bool audioEnabled = false;
    bool vmmode = false;
    bool debugMode = false;
    bool shaderDebug = true;
    bool uiLayoutLocked = true;
    bool uiShowSettingsClip = true;
    bool uiShowSettingsArp = true;
};
struct appsettings {
    uint32_t fileFmtVersion = 0;
    app_daw_settings dawsettings;
    app_autosave_settings autosave;
    app_plugin_configuration pluginsettings;
    app_iosettings iosettings;
    app_path_remapping pathmapping;
    recentfilelist recentfiles;
    std::vector<appwindowsettings> windowSettings = {appwindowsettings{}};
    bool saveOnExit = true;
};

void saveSettings(appsettings& _settings);
void loadSettings(appsettings& settings);
