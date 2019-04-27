#include "themefile.h"
#include <cereal/cereal.hpp>
#include <cereal/cereal_optional_nvp.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/cereal_optional_nvp.hpp>
#include "str_util.h"
#include "color_util.h"
#include "config.h"
#include "theme.h"
#include "guiconstant.h"
#include "guicolors.h"
#include "msgbox.h"
#include <math.h>
#include <chrono>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <assert.h>
#include <algorithm>
#include <map>
using namespace std;
using namespace cereal;


namespace GuiColor {
constant_t getConstantById(int32_t id);
constant_t getConstantByName(String name);
}
namespace GuiConstant {
constant_t getConstantById(int32_t id);
constant_t getConstantByName(String name);
}

struct theme_data {
	std::unordered_map<String,int32_t> mapColors;
	std::unordered_map<String,int32_t> mapProperties;
};

void storeThemeData(guitheme_t const & m, theme_data& out) {
	for (auto it = m.mapColors.begin(); it != m.mapColors.end(); ++it) {
		int32_t key = it->first;
		GuiColor::constant_t c = GuiColor::getConstantById(key);
		if (c.idx <= 0)
			continue;
		out.mapColors[c.name] = it->second;
	}
	for (auto it = m.mapProperties.begin(); it != m.mapProperties.end(); ++it) {
		int32_t key = it->first;
		GuiConstant::constant_t c = GuiConstant::getConstantById(key);
		if (c.idx <= 0)
			continue;
		out.mapProperties[c.name] = it->second;
	}

}
void loadThemeData(theme_data& data, guitheme_t& out) {
	out.mapColors.clear();
	out.mapProperties.clear();
	for (auto it = data.mapColors.begin(); it != data.mapColors.end(); ++it) {
		String key = it->first;
		GuiColor::constant_t c = GuiColor::getConstantByName(key);
		if (c.idx <= 0) {
			continue;
		}
		out.mapColors[c.idx] = it->second;
		if (c.idx < out.vecNVGColors.size()) {
			out.vecNVGColors[c.idx] = rgbaToNvg(it->second);
		} else {
			assert(0);
		}

	}
	for (auto it = data.mapProperties.begin(); it != data.mapProperties.end(); ++it) {
		String key = it->first;
		GuiConstant::constant_t c = GuiConstant::getConstantByName(key);
		assert(c.idx > 0);
		out.mapProperties[c.idx] = it->second;
	}
}

template<class Archive>
void serialize(Archive & archive, NVGcolor & m)
{
	archive(make_nvp("r", m.r), make_nvp("g", m.g), make_nvp("b", m.b), make_nvp("a", m.a));
}

template<class Archive>
void serialize(Archive & archive, theme_data & m)
{
	archive(make_nvp("colors", m.mapColors));
	archive(make_nvp("properties", m.mapProperties));
}
template<class Archive>
void save(Archive & archive, guitheme_t const & m)
{
	archive(make_nvp("name", m.name));
	archive(make_nvp("colorBg", m.colorBg));
	archive(make_nvp("colorBgStroke", m.colorBgStroke));
	archive(make_nvp("colorBgHover", m.colorBgHover));
	archive(make_nvp("colorBgPressed", m.colorBgPressed));
	archive(make_nvp("colorBgFocused", m.colorBgFocused));
	archive(make_nvp("colorBgDisabled", m.colorBgDisabled));
	archive(make_nvp("colorBgFrameBase", m.colorBgFrameBase));
	archive(make_nvp("colorBgFrameOutline", m.colorBgFrameOutline));
	archive(make_nvp("colorBgFrameHighlight", m.colorBgFrameHighlight));
	archive(make_nvp("colorBgFrameBright", m.colorBgFrameBright));
	//save new
	theme_data data;
	storeThemeData(m, data);
	archive(make_nvp("data", data));
}

template<class Archive>
void load(Archive & archive, guitheme_t & m)
{
	archive(make_nvp("name", m.name));
	archive(make_nvp("colorBg", m.colorBg));
	archive(make_nvp("colorBgStroke", m.colorBgStroke));
	archive(make_nvp("colorBgHover", m.colorBgHover));
	archive(make_nvp("colorBgPressed", m.colorBgPressed));
	archive(make_nvp("colorBgFocused", m.colorBgFocused));
	archive(make_nvp("colorBgDisabled", m.colorBgDisabled));
	archive(make_nvp("colorBgFrameBase", m.colorBgFrameBase));
	archive(make_nvp("colorBgFrameOutline", m.colorBgFrameOutline));
	archive(make_nvp("colorBgFrameHighlight", m.colorBgFrameHighlight));
	archive(make_nvp("colorBgFrameBright", m.colorBgFrameBright));

	const char* namePtr = archive.getNodeName();
    if (namePtr && strcmp(namePtr, "mapValues") == 0) {
    	//load old

    	std::unordered_map<String,int32_t> mapValues;
    	archive(make_nvp("mapValues", mapValues));
    	archive(make_nvp("mapProperties", m.mapProperties));
    	std::unordered_map<int32_t,int32_t> mapColors;
    	for (auto it = mapValues.begin(); it != mapValues.end(); ++it) {
    		String key = it->first;
    		GuiColor::constant_t c = GuiColor::getConstantByName(key);
    		if (c.idx <= 0)
    			continue;
    		mapColors[c.idx] = it->second;
    	}
    	if (m.vecNVGColors.size() != NUM_GUI_COLORS) {
    		m.vecNVGColors.resize(NUM_GUI_COLORS);
    	}
    	for (auto it = mapColors.begin(); it != mapColors.end(); ++it) {
    		if (it->first < 0 || it->first >= NUM_GUI_COLORS)
    			continue;
    		m.vecNVGColors[it->first] = rgbaToNvg(it->second);
    	}
    	m.mapColors = mapColors;
    } else {
    	//load new
    	theme_data data;
    	archive(make_nvp("data", data));
    	loadThemeData(data, m);
    }
}
template<class Archive>
void serialize(Archive & archive, themefile & m)
{
	archive(make_nvp("current", m.theme), make_nvp("themes", m.themes));
}
themefile loadThemeFile() {
	Stringstream ss;
	ifstream file(THEMEFILE_NAME, ifstream::in);
	if (file) {
	    ss << file.rdbuf();
	    std::streampos length = file.tellg();
	    if (length > 10) {
	    	themefile tmpSettings;
		    cereal::JSONInputArchive ar(ss);
		    ar( tmpSettings );
		    return tmpSettings;
	    }
	}
	throw std::runtime_error("Failed reading file");
}
void saveThemeFile(themefile& _settings) {
	ofstream file;
	file.exceptions(~ofstream::goodbit);
	file.open(THEMEFILE_NAME, ofstream::out);
    cereal::JSONOutputArchive ar( file );
    ar( _settings );
}
