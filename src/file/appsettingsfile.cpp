#include "appsettings.h"
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/details/helpers.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal_optional_nvp/cereal_optional_nvp.hpp>
#include <fstream>

#include "config.h"
#include "exceptions.h"
#include "fileio.h"
#include "host/audio_config.h"
#include "platform.h"
#include "str_util.h"

using namespace cereal;


#include "platform/win/windowsize.h"


template<class Archive>
void save(Archive& archive, appwindow_size_t const& settings, const std::uint32_t version) {
    size_type size = sizeof(settings.data);
    archive(make_nvp("valid", settings.valid), make_nvp("type", settings.type), make_nvp("size", size));
    ((JSONOutputArchive*) &archive)->saveBinaryValue(settings.data, size, "data");
}

template<class Archive>
void load(Archive& ar, appwindow_size_t& settings, const std::uint32_t version) {
    if (version < 1) {
        settings = {};
        settings.valid = false;
        return;
    }
    size_type size = 0;
    ar(make_nvp("valid", settings.valid), make_nvp("type", settings.type), make_nvp("size", size));
    std::vector<std::byte> vec;
    vec.resize(size);
    ((JSONInputArchive*) &ar)->loadBinaryValue((void*) vec.data(), size, "data");
    memcpy(&settings.data[0], vec.data(), size);
}

namespace DAW::AudioIO {
    template <class Archive>
    void serialize(Archive& ar, io_cfg_tracks& cfg) {
        ar(make_nvp("isInit", cfg.isInit), make_nvp("inputs", cfg.input), make_nvp("outputs", cfg.output));
    }
    template <class Archive>
    void serialize(Archive& ar, io_cfg_channel& settings) {
        try {
            ar(make_nvp("idx", settings.idx),
                make_nvp("offset", settings.offset),
                make_nvp("name", settings.name),
                make_nvp("type", settings.type));
        } catch (const std::runtime_error&) {
            ar(make_nvp("idx", settings.idx),
                make_nvp("channelOffset", settings.offset),
                make_nvp("name", settings.name),
                make_nvp("type", settings.type));
        }
    }
} // namespace DAW::AudioIO

template <class Archive>
void serialize(Archive& ar, io_channel& cfg) {
    ar(make_nvp("channels", cfg.channels), make_nvp("idx", cfg.idx));
}
template <class Archive>
void serialize(Archive& ar, midi_channel& cfg) {
    ar(make_nvp("deviceName", cfg.deviceName), make_nvp("channels", cfg.channels), make_nvp("idx", cfg.idx));
}
template <class Archive>
void serialize(Archive& ar, app_ioasioconfig& cfg) {
    ar(make_nvp("deviceName", cfg.deviceName), make_nvp("inputs", cfg.inputs), make_nvp("outputs", cfg.outputs));
}
template <class Archive>
void serialize(Archive& ar, app_ioaudioconfig& cfg) {
    ar(make_nvp("deviceAPI", cfg.device_api),
       make_nvp("deviceNameInput", cfg.deviceNameInput),
       make_nvp("deviceNameOutput", cfg.deviceNameOutput));
}
template <class Archive>
void serialize(Archive& ar, app_iomidiconfig& cfg) {
    ar(make_nvp("deviceAPI", cfg.device_api), make_nvp("inputs", cfg.inputs), make_nvp("outputs", cfg.outputs));
}
template <class Archive>
void serialize(Archive& ar, app_iosettings& settings) {
    ar(make_nvp("samplerate", settings.samplerate),
       make_nvp("blocksize", settings.blocksize),
       make_nvp("device_api", settings.device_api));
    make_optional_nvp(ar, "io_midi", settings.midiconfigs);
    make_optional_nvp(ar, "io_audio", settings.configs);
    make_optional_nvp(ar, "io_asio", settings.asioConfig);
    make_optional_nvp(ar, "io_channels", settings.channelConfigs);
    make_optional_nvp(ar, "internalSamplerate", settings.internalSamplerate);
    make_optional_nvp(ar, "internalBlocksize", settings.internalBlocksize);
}
template <class Archive>
void serialize(Archive& ar, recentfilelistentry& f) {
    ar(make_nvp("path", f.path), make_nvp("date", f.date));
}
template <class Archive>
void serialize(Archive& ar, recentfilelist& recentfiles) {
    ar(make_nvp("sorted", recentfiles.sortedEntries), make_nvp("files", recentfiles.recentFilesMeta));
}

template <class Archive>
void serialize(Archive& ar, app_vst2_config& settings) {
    ar(make_nvp("uidRemapping", settings.uidRemapping));
}

template <class Archive>
void serialize(Archive& ar, app_plugin_configuration& settings) {
    ar(
        make_nvp("vst2.path", settings.pathVst2),
        make_nvp("vst2.config", settings.configVst2)
    );
    make_optional_nvp(ar, "clap.path", settings.pathClap);
    make_optional_nvp(ar, "vst3.path", settings.pathVst3);
}

template <class Archive>
void serialize(Archive& ar, appwindowsettings& settings) {
    ar(
        make_nvp("grid", settings.dens),
        make_nvp("windowsize", settings.size),
        make_nvp("zoom", settings.zoom),
        make_nvp("flags", settings.flags)
    );
}

template <class Archive>
void serialize(Archive& ar, app_path_remapping& settings) {
    ar(make_nvp("pathRemapping", settings.pathRemapping));
}

template <class Archive>
void serialize(Archive& ar, app_autosave_settings& settings) {
    ar(make_nvp("saveDelay", settings.tmSaveDelayMinutes));
    ar(make_nvp("reminderDelay", settings.tmReminderDelayMinutes));
}

template <class Archive>
void serialize(Archive& ar, app_daw_settings& settings) {
    ar(
        make_nvp("startupProjectPath", settings.startupProjectPath),
        make_nvp("startupLoadDeffered", settings.startupLoadDeffered),
        make_nvp("startupAudioEnabled", settings.audioEnabled),
        make_nvp("vmmode", settings.vmmode),
        make_nvp("debugMode", settings.debugMode),
        make_nvp("shaderDebug", settings.shaderDebug)
    );
    make_optional_nvp(ar, "uiLayoutLocked", settings.uiLayoutLocked);
    make_optional_nvp(ar, "uiShowSettingsArp", settings.uiShowSettingsArp);
    make_optional_nvp(ar, "uiShowSettingsClip", settings.uiShowSettingsClip);
    make_optional_nvp(ar, "lowLatencyMode", settings.lowLatencyMode);
}

template<class Archive>
void save(Archive& ar, appsettings const& settings, const std::uint32_t version) {
    ar(
        make_nvp("dawsettings", settings.dawsettings),
        make_nvp("autosave", settings.autosave),
        make_nvp("pluginsettings", settings.pluginsettings),
        make_nvp("iosettings", settings.iosettings),
        make_nvp("pathmapping", settings.pathmapping),
        make_nvp("recentfiles", settings.recentfiles),
        make_nvp("windowSettings", settings.windowSettings),
        make_nvp("theme", settings.selectedTheme),
        make_nvp("userlibraries", settings.userLibraryPaths)
    );
}

template<class Archive>
void load(Archive& ar, appsettings& settings, const std::uint32_t version) {
    settings.fileFmtVersion = version;
    ar(
        make_nvp("dawsettings", settings.dawsettings),
        make_nvp("autosave", settings.autosave),
        make_nvp("pluginsettings", settings.pluginsettings),
        make_nvp("iosettings", settings.iosettings),
        make_nvp("pathmapping", settings.pathmapping),
        make_nvp("recentfiles", settings.recentfiles),
        make_nvp("windowSettings", settings.windowSettings),
        make_nvp("theme", settings.selectedTheme)
    );
    if (version >= 5) {
        ar(
            make_nvp("userlibraries", settings.userLibraryPaths)
        );
    }
}
CEREAL_CLASS_VERSION(appwindow_size_t, 1);
CEREAL_CLASS_VERSION(appsettings, 5);


void loadSettings(appsettings& settings) {
    Stringstream ss;
    String cwdPathSettings = App::Platform::toUserdataPath(SETTINGS_NAME);
    if (!FileExists(cwdPathSettings))
        return;
    std::ifstream file(cwdPathSettings, std::ifstream::in);
    if (file) {
        ss << file.rdbuf();
        std::streampos length = file.tellg();
        if (length > 10) {
            cereal::JSONInputArchive ar(ss);
            appsettings tmpSettings;
            ar(tmpSettings);
            settings = tmpSettings;
            return;
        }
    }
    throw FileIOException("Failed reading application settings file " + cwdPathSettings);
}

void saveSettings(appsettings& _settings) {
    String cwdPathSettings = App::Platform::toUserdataPath(SETTINGS_NAME);
    std::ofstream file;
    file.exceptions(~std::ofstream::goodbit);
    file.open(cwdPathSettings, std::ofstream::out);
    cereal::JSONOutputArchive ar(file);
    ar(_settings);
}
