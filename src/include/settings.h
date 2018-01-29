#pragma once
#include <cereal/cereal.hpp>
#include <cereal/cereal_optional_nvp.hpp>
#include "str_util.h"
#include "grid.h"
#ifdef _WIN32

#include <windows.h>
struct windowsize
{
public:
	bool valid;
	WINDOWPLACEMENT p;
	windowsize() {
		p.length = sizeof(WINDOWPLACEMENT);
		valid = false;
	}
	windowsize(HWND hwnd) {
		p.length = sizeof(WINDOWPLACEMENT);
		valid = true;
		GetWindowPlacement(hwnd, &p);
	}
	template<class Archive>
	void serialize(Archive & ar) {
		  ar(valid, p.flags,
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
	void apply(HWND hwnd) {
		if (valid) {
			SetWindowPlacement(hwnd, &p);
		}
	}
};
#endif
struct appsettings
{
#ifdef _WIN32
	windowsize size;
#endif
	grid_density dens;
	String device_api;
	String device_selected;
	bool startEngine = false;
	String pluginPath;
public:
	appsettings() { }
	template<class Archive>
	void serialize(Archive & ar) {
		ar(cereal::make_nvp("grid", dens),
			cereal::make_nvp("device_api", device_api),
			cereal::make_nvp("device_selected", device_selected));
		make_optional_nvp(ar, "startEngine", startEngine);
#ifdef _WIN32
		make_optional_nvp(ar, "window", size);
#endif
		make_optional_nvp(ar, "pluginPath", pluginPath);
	}
};
extern appsettings settings;
void saveSettings(appsettings& _settings);
bool loadSettings(appsettings& _settings);
