#include "appsettings.h"
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal_optional_nvp/cereal_optional_nvp.hpp>
#include <fstream>

#include "config.h"
#include "exceptions.h"
#include "host/audio_config.h"
#include "platform.h"
#include "str_util.h"

using namespace cereal;


#ifdef _WIN32
#include "platform/win/windowsize.h"

template <class Archive>
void serialize(Archive& ar, appwindow_size_t& settings) {
    auto& p = settings.p;
    ar(settings.valid,
       p.flags,
       p.showCmd,
       (int32_t&) p.ptMinPosition.x,
       (int32_t&) p.ptMinPosition.y,
       (int32_t&) p.ptMaxPosition.x,
       (int32_t&) p.ptMaxPosition.y,
       (int32_t&) p.rcNormalPosition.left,
       (int32_t&) p.rcNormalPosition.top,
       (int32_t&) p.rcNormalPosition.right,
       (int32_t&) p.rcNormalPosition.bottom);
}
#endif
#ifdef __linux__
#include "platform/linux/windowsize.h"

template <class Archive>
void serialize(Archive& ar, appwindow_size_t& settings) {
    ar(settings.valid,
       settings.x,
       settings.y,
       settings.w,
       settings.h,
       settings.hmax,
       settings.vmax);
}
#endif

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
}
template <class Archive>
void serialize(Archive& ar, appwindowsettings& settings) {
    ar(make_nvp("grid", settings.dens));
    make_optional_nvp(ar, "windowsize", settings.size);
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
    make_optional_nvp(ar, "globalZoom", settings.globalZoom);
    make_optional_nvp(ar, "uiLayoutLocked", settings.uiLayoutLocked);
    make_optional_nvp(ar, "uiShowSettingsArp", settings.uiShowSettingsArp);
    make_optional_nvp(ar, "uiShowSettingsClip", settings.uiShowSettingsClip);
}

template<class Archive>
void load(Archive& ar, appsettings& settings, const std::uint32_t version) {
    settings.fileFmtVersion = version;
    if (version < 2) {
        bool bAudioEnabled = false;
        bool bVMMode = false;
        ar(
            make_nvp("window.main", settings.wndMain), 
            make_nvp("window.companion", settings.wndCompanion),
            make_nvp("io", settings.iosettings),
            make_nvp("startengine", bAudioEnabled),
            make_nvp("plugins", settings.pluginsettings),
            make_nvp("vmmode", bVMMode),
            make_nvp("recentfiles", settings.recentfiles)
        );
        bool bShaderDebug = false;
        make_optional_nvp(ar, "pathmapping", settings.pathmapping);
        make_optional_nvp(ar, "shaderDebug", bShaderDebug);
        make_optional_nvp(ar, "autosave", settings.autosave);
        settings.dawsettings.shaderDebug = bShaderDebug;
        settings.dawsettings.audioEnabled = bAudioEnabled;
    } else {
        ar(
            make_nvp("dawsettings", settings.dawsettings),
            make_nvp("autosave", settings.autosave),
            make_nvp("pluginsettings", settings.pluginsettings),
            make_nvp("iosettings", settings.iosettings),
            make_nvp("pathmapping", settings.pathmapping),
            make_nvp("recentfiles", settings.recentfiles),
            make_nvp("wndMain", settings.wndMain),
            make_nvp("wndCompanion", settings.wndCompanion)
        );
    }
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
        make_nvp("wndMain", settings.wndMain),
        make_nvp("wndCompanion", settings.wndCompanion)
    );
}
CEREAL_CLASS_VERSION(appsettings, 2);


void loadSettings(appsettings& settings) {
    Stringstream ss;
    String cwdPathSettings = App::Platform::toUserdataPath(SETTINGS_NAME);
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
