#include "appsettings.h"
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/map.hpp>
#include <cereal/cereal_optional_nvp.hpp>
#include <fstream>

#include "config.h"
#include "host/audio_config.h"
#include "platform.h"
#include "str_util.h"

using namespace cereal;

using std::ifstream;
using std::ofstream;

#ifdef _WIN32
#include "platform/win/windowsize.h"

template <class Archive>
void serialize(Archive& ar, windowsize& settings) {
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

namespace AudioIO {
    template <class Archive>
    void serialize(Archive& ar, io_cfg_tracks& cfg) {
        ar(make_nvp("isInit", cfg.isInit), make_nvp("inputs", cfg.input), make_nvp("outputs", cfg.output));
    }
    template <class Archive>
    void serialize(Archive& ar, io_cfg_channel& settings) {
        ar(make_nvp("idx", settings.idx),
           make_nvp("channelOffset", settings.channelOffset),
           make_nvp("name", settings.name),
           make_nvp("type", settings.type));
    }
} // namespace AudioIO
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
void serialize(Archive& ar, appsettings& settings) {
    ar(make_nvp("grid", settings.wndMain.dens), make_nvp("iosettings", settings.iosettings));
    make_optional_nvp(ar, "startEngine", settings.startEngine);
#ifdef _WIN32
    make_optional_nvp(ar, "window", *(settings.wndMain.size));
    make_optional_nvp(ar, "window2", *(settings.wndCompanion.size));
#endif
    make_optional_nvp(ar, "window2grid", settings.wndCompanion.dens);
    make_optional_nvp(ar, "pluginPath", settings.pluginPath);
    make_optional_nvp(ar, "vmmode", settings.vmmode);
    make_optional_nvp(ar, "recentfiles", settings.recentfiles);
}

appsettings loadSettings() {
    Stringstream ss;
    String cwdPathSettings = toUserdataPath(SETTINGS_NAME);
    ifstream file(cwdPathSettings, ifstream::in);
    if (file) {
        ss << file.rdbuf();
        std::streampos length = file.tellg();
        if (length > 10) {
            cereal::JSONInputArchive ar(ss);
            appsettings tmpSettings;
            ar(tmpSettings);
            return tmpSettings;
        }
    }
    throw std::runtime_error("Failed reading application settings file " + cwdPathSettings);
}

void saveSettings(appsettings& _settings) {
    String cwdPathSettings = toUserdataPath(SETTINGS_NAME);
    ofstream file;
    file.exceptions(~ofstream::goodbit);
    file.open(cwdPathSettings, ofstream::out);
    cereal::JSONOutputArchive ar(file);
    ar(_settings);
}
