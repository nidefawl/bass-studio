#pragma once
#include "str_util.h"
#include "grid.h"
#include "host/audio_config.h"
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
	String device_api = "ASIO";
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
	int32_t samplerate = 44100;
	int32_t blocksize = 256;
	app_ioasioconfig asioConfig;
	std::map<String, app_ioaudioconfig> configs;
	std::map<String, app_iomidiconfig> midiconfigs;
	std::map<String, AudioIO::io_cfg_tracks> channelConfigs;
	String device_api;
	app_ioasioconfig& getAsioConfig() {
    	return asioConfig;
    }
	AudioIO::io_cfg_tracks& getChannelConfig(String devApi) {
    	if (!channelConfigs.count(devApi)) {
    		channelConfigs[devApi] = AudioIO::io_cfg_tracks();
    	}
    	return channelConfigs[devApi];
    }
	app_ioaudioconfig& getConfig(String devApi) {
    	if (!configs.count(devApi)) {
    		configs[devApi] = app_ioaudioconfig();
    	}
    	return configs[devApi];
    }
	app_iomidiconfig& getIOConfigMidi(String devApi) {
    	if (!midiconfigs.count(devApi)) {
    		midiconfigs[devApi] = app_iomidiconfig();
    	}
    	return midiconfigs[devApi];
    }
	bool isAsio() {
		return device_api == asioConfig.device_api;
	}
};
struct appsettings
{
#ifdef _WIN32
    std::unique_ptr<windowsize> size;
#endif
	grid_density dens;
//	String device_selected;
	bool startEngine = false;
	bool vmmode = false;
	String pluginPath;
	app_iosettings iosettings;
public:
	~appsettings();
	appsettings();
	appsettings(const appsettings& other);
    appsettings& operator=(const appsettings& other);
    appsettings(appsettings&& other);
    appsettings& operator=(appsettings&& other);
};
extern appsettings settings;
void saveSettings(appsettings& _settings);
appsettings loadSettings();
