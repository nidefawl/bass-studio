#pragma once
#include <Windows.h>
#include <cereal/cereal.hpp>
#include <cereal/cereal_optional_nvp.hpp>
#include "str_util.h"
#include "grid.h"

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
struct appsettings
{
	windowsize size;
	grid_density dens;
	String device_api;
	String device_selected;
	bool startEngine = false;
public:
	appsettings() { }
	template<class Archive>
	void serialize(Archive & ar) {
		ar(cereal::make_nvp("window", size),
				cereal::make_nvp("grid", dens),
				cereal::make_nvp("device_api", device_api),
				cereal::make_nvp("device_selected", device_selected));
	   CEREAL_OPTIONAL_NVP(ar, startEngine);
	}
};
extern appsettings settings;
