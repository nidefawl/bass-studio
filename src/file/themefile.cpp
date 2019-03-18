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
#include "gui/theme.h"
#include "msgbox.h"
#include <math.h>
#include <chrono>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>
using namespace std;
using namespace cereal;
template<class Archive>
void serialize(Archive & archive, NVGcolor & m)
{
	archive(make_nvp("r", m.r), make_nvp("g", m.g), make_nvp("b", m.b), make_nvp("a", m.a));
}
//template<class Archive>
//void save(Archive & archive,
//		guitheme_t const & m)
//{
//  archive(make_nvp("colorBg", m.colorBg));
//}
//
//template<class Archive>
//void load(Archive & archive,
//		guitheme_t & m)
//{
//  archive(make_nvp("colorBg", m.colorBg));
//}
//template<class Archive>
//void serialize(Archive & archive, guitheme_t const & m)
//{
//	archive(make_nvp("colorBg", m.colorBg));
//	archive(make_nvp("colorBgStroke", m.colorBgStroke));
//	archive(make_nvp("colorBgHover", m.colorBgHover));
//	archive(make_nvp("colorBgPressed", m.colorBgPressed));
//	archive(make_nvp("colorBgFocused", m.colorBgFocused));
//	archive(make_nvp("colorBgDisabled", m.colorBgDisabled));
//	archive(make_nvp("colorBgFrameBase", m.colorBgFrameBase));
//	archive(make_nvp("colorBgFrameOutline", m.colorBgFrameOutline));
//	archive(make_nvp("colorBgFrameHighlight", m.colorBgFrameHighlight));
//	archive(make_nvp("colorBgFrameBright", m.colorBgFrameBright));
//	archive(make_nvp("guiColors", m.vecNVGColors));
//	archive(make_nvp("mapColors", m.mapColors));
//	archive(make_nvp("mapProperties", m.mapProperties));
//}
namespace GuiColor {
constant_t getConstantById(int32_t id);
constant_t getConstantByName(String name);
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

	std::unordered_map<String,int32_t> mapValues;
	for (auto it = m.mapColors.begin(); it != m.mapColors.end(); ++it) {
		int32_t key = it->first;
		GuiColor::constant_t c = GuiColor::getConstantById(key);
		assert(c.idx > 0);
//		assert(c.name.length());
		mapValues[c.name] = it->second;

	}
	archive(make_nvp("mapValues", mapValues));
	archive(make_nvp("mapProperties", m.mapProperties));

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
//	archive(make_nvp("values", m.values));
}
template<class Archive>
void serialize(Archive & archive, themefile & m)
{
	archive(make_nvp("current", m.theme), make_nvp("themes", m.themes));
}
bool loadThemeFile(themefile& _settings) {
	try {
		Stringstream ss;
		ifstream file(THEMEFILE_NAME, ifstream::in);
		if (file) {
		    ss << file.rdbuf();
		    std::streampos length = file.tellg();
		    if (length > 10) {
			    cereal::JSONInputArchive ar(ss);
			    ar( _settings );
			    return true;
		    }
		}
		saveThemeFile(_settings);
	} catch (std::exception& e) {
	/*	ngui::show("Couldn't read config file.\nSome settings may have been reset", "Warning", ngui::Style::Warning, ngui::Buttons::OK);
		std::cout << e.what();
		std::cout << std::endl;*/
		_settings = themefile();
	}
	return false;
}
void saveThemeFile(themefile& _settings) {
	ofstream file;
	file.exceptions(~ofstream::goodbit);
	try {
		file.open(THEMEFILE_NAME, ofstream::out);
	    cereal::JSONOutputArchive ar( file );
	    ar( _settings );
	} catch (std::exception& e) {
		std::cout << "Failed writing settings\n";
		std::cout << e.what();
		std::cout << std::endl;
	}
}
