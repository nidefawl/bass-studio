#pragma once
#include "str_util.h"
#include "grid.h"

#ifdef _WIN32
struct windowsize;
#endif
struct appsettings
{
#ifdef _WIN32
	windowsize* size;
#endif
	grid_density dens;
	String device_api;
	String device_selected;
	bool startEngine = false;
	String pluginPath;
public:
	appsettings();
	~appsettings();
};
extern appsettings settings;
void saveSettings(appsettings& _settings);
bool loadSettings(appsettings& _settings);
