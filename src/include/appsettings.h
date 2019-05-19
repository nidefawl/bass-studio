#pragma once
#include "str_util.h"
#include "grid.h"
#include <map>

#ifdef _WIN32
struct windowsize;
#endif
struct app_io {
	int idx;
	String deviceName;
};
struct app_ioconfig {
	String device_api;
	std::vector<app_io> outputs;
	std::vector<app_io> inputs;
};
struct app_iosettings {
	int32_t samplerate = 44100;
	int32_t blocksize = 256;
	std::map<String, app_ioconfig> configs;
	std::map<String, app_ioconfig> midiconfigs;
	String device_api;
	app_ioconfig& getConfig(String devApi) {
    	if (!configs.count(devApi)) {
    		configs[devApi] = app_ioconfig();
    	}
    	return configs[devApi];
    }
	app_ioconfig& getIOConfigMidi(String devApi) {
    	if (!configs.count(devApi)) {
    		configs[devApi] = app_ioconfig();
    	}
    	return configs[devApi];
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
