#include "appsettings.h"
#include <fstream>
#include <sstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/map.hpp>
#include <cereal/cereal_optional_nvp.hpp>

#include "config.h"
#include "str_util.h"
#include "exceptions.h"
#include "msgbox.h"
#include "host/audio_config.h"

using namespace cereal;

using std::ifstream;
using std::ofstream;

#ifdef _WIN32
#include "platform/win/windowsize.h"

template<class Archive>
void serialize(Archive & ar, windowsize& settings) {
	auto& p = settings.p;
	ar(settings.valid, p.flags,
		  p.showCmd,
		  p.ptMinPosition.x,
		  p.ptMinPosition.y,
		  p.ptMaxPosition.x,
		  p.ptMaxPosition.y,
		  p.rcNormalPosition.left,
		  p.rcNormalPosition.top,
		  p.rcNormalPosition.right,
		  p.rcNormalPosition.bottom);
}
#endif
namespace AudioIO {
template<class Archive>
void serialize(Archive & ar, io_cfg_tracks& cfg) {
	ar(cereal::make_nvp("isInit", cfg.isInit),
	cereal::make_nvp("inputs", cfg.input),
	cereal::make_nvp("outputs", cfg.output));
}
template<class Archive>
void serialize(Archive & ar, io_cfg_channel& settings) {
	using cereal::make_nvp;
	ar(make_nvp("idx", settings.idx),
		make_nvp("channelOffset", settings.channelOffset),
		make_nvp("name", settings.name),
		make_nvp("type", settings.type));
}
}
template<class Archive>
void serialize(Archive & ar, io_channel& cfg) {
	ar(cereal::make_nvp("channels", cfg.channels),
		cereal::make_nvp("idx", cfg.idx));
}
template<class Archive>
void serialize(Archive & ar, midi_channel& cfg) {
	ar(cereal::make_nvp("deviceName", cfg.deviceName),
		cereal::make_nvp("channels", cfg.channels),
		cereal::make_nvp("idx", cfg.idx));
}
template<class Archive>
void serialize(Archive & ar, app_ioasioconfig& cfg) {
	ar(cereal::make_nvp("deviceName", cfg.deviceName),
		cereal::make_nvp("inputs", cfg.inputs),
		cereal::make_nvp("outputs", cfg.outputs));
}
template<class Archive>
void serialize(Archive & ar, app_ioaudioconfig& cfg) {
	ar(cereal::make_nvp("deviceAPI", cfg.device_api),
	cereal::make_nvp("deviceNameInput", cfg.deviceNameInput),
	cereal::make_nvp("deviceNameOutput", cfg.deviceNameOutput));
}
template<class Archive>
void serialize(Archive & ar, app_iomidiconfig& cfg) {
	ar(cereal::make_nvp("deviceAPI", cfg.device_api),
	cereal::make_nvp("inputs", cfg.inputs),
	cereal::make_nvp("outputs", cfg.outputs));
}
template<class Archive>
void serialize(Archive & ar, app_iosettings& settings) {
	using cereal::make_nvp;
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
template<class Archive>
void serialize(Archive & ar, recentfilelistentry& f) {
	ar(cereal::make_nvp("path", f.path),
		cereal::make_nvp("date", f.date));

}
template<class Archive>
void serialize(Archive & ar, recentfilelist& recentfiles) {
	ar(cereal::make_nvp("sorted", recentfiles.sortedEntries),
		cereal::make_nvp("files", recentfiles.recentFilesMeta));
}
template<class Archive>
void serialize(Archive & ar, appsettings& settings) {
	ar(cereal::make_nvp("grid", settings.wndMain.dens),
		cereal::make_nvp("iosettings", settings.iosettings));
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
	ifstream file(SETTINGS_NAME, ifstream::in);
	if (file) {
		ss << file.rdbuf();
		std::streampos length = file.tellg();
		if (length > 10) {
			cereal::JSONInputArchive ar(ss);
			appsettings tmpSettings;
			ar( tmpSettings );
			return tmpSettings;
		}
	}
	throw std::runtime_error("Failed reading config");
}
void saveSettings(appsettings& _settings) {
	ofstream file;
	file.exceptions(~ofstream::goodbit);
	file.open(SETTINGS_NAME, ofstream::out);
    cereal::JSONOutputArchive ar( file );
    ar( _settings );
}
