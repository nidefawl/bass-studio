#pragma once
#include "config.h"
#include "grid.h"
#include "host/audio_config.h"
#include "str_util.h"
#include <ctime>
#include <iostream>
#include <map>


#ifdef _WIN32
struct windowsize;
#endif

struct io_channel {
    int idx;
    std::vector<int32_t> channels;
};

struct midi_channel {
    int idx;
    String deviceName;
    std::vector<int32_t> channels;
};

struct app_ioaudioconfig {
    String device_api;
    String deviceNameInput;
    String deviceNameOutput;
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
    uint32_t samplerate = 44100;
    uint32_t blocksize = 256;
    uint32_t internalSamplerate = 44100;
    uint32_t internalBlocksize = 256;
    app_ioasioconfig asioConfig;
    std::map<String, app_ioaudioconfig> configs;
    std::map<String, app_iomidiconfig> midiconfigs;
    std::map<String, AudioIO::io_cfg_tracks> channelConfigs;
    String device_api;
    app_ioasioconfig& getAsioConfig() { return asioConfig; }
    AudioIO::io_cfg_tracks& getChannelConfig(const String& devApi) {
        if (!channelConfigs.count(devApi)) {
            channelConfigs[devApi] = AudioIO::io_cfg_tracks();
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
#ifdef _WIN32
    std::unique_ptr<windowsize> size;
#endif
    grid_density dens;
    appwindowsettings() noexcept;
    ~appwindowsettings() = default;
    appwindowsettings(const appwindowsettings& other);
    appwindowsettings& operator=(const appwindowsettings& other);
    appwindowsettings(appwindowsettings&& other) = default;
    appwindowsettings& operator=(appwindowsettings&& other) = default;
};

struct recentfilelistentry {
    String path;
    String date;
};

class recentfilelist {
public:
    std::vector<String> sortedEntries;
    std::map<String, recentfilelistentry> recentFilesMeta;
    void add(const String& path) {
        while (sortedEntries.size() > 31) {
            String& s = sortedEntries.back();
            auto it = recentFilesMeta.find(s);
            if (it != recentFilesMeta.end()) {
                recentFilesMeta.erase(it);
            }
            sortedEntries.pop_back();
        }
        auto it = sortedEntries.begin();
        while (it != sortedEntries.end()) {
            String& s = *it;
            if (s == path) {
                it = sortedEntries.erase(it);
            } else {
                it++;
            }
        }

        sortedEntries.insert(sortedEntries.begin(), path);

        std::time_t t = std::time(nullptr); // get time now
        std::tm* now = std::localtime(&t);
        auto strDate = std::to_string(now->tm_year + 1900) +
                       "-" + std::to_string(now->tm_mon + 1) +
                       "-" + std::to_string(now->tm_mday) + "\n";
        recentFilesMeta[path] = recentfilelistentry{path, strDate};
    }
};

struct appsettings {
    appsettings() noexcept = default; // need noexcept for static storage
    appwindowsettings wndMain;
    appwindowsettings wndCompanion;
    //String device_selected;
    bool startEngine = false;
    bool vmmode = false;
    String pluginPath;
    app_iosettings iosettings;
    recentfilelist recentfiles;
};

#if HAS_APP_SETTINGS
namespace DAW {
    extern appsettings settings;
}
#endif

void saveSettings(appsettings& _settings);
appsettings loadSettings();
