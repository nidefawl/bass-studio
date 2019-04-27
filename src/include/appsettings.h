#pragma once
#include "str_util.h"
#include "grid.h"

#ifdef _WIN32
struct windowsize;
#endif
struct appsettings
{
#ifdef _WIN32
    std::unique_ptr<windowsize> size;
#endif
	grid_density dens;
	String device_api;
	String device_selected;
	bool startEngine = false;
	bool vmmode = false;
	String pluginPath;
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
