#include "appsettings.h"
#include <fstream>
#include <sstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/cereal_optional_nvp.hpp>

#include "config.h"
#include "str_util.h"
#include "exceptions.h"
#include "msgbox.h"

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

template<class Archive>
void serialize(Archive & ar, appsettings& settings) {
	ar(cereal::make_nvp("grid", settings.dens),
		cereal::make_nvp("device_api", settings.device_api),
		cereal::make_nvp("device_selected", settings.device_selected));
	make_optional_nvp(ar, "startEngine", settings.startEngine);
#ifdef _WIN32
	make_optional_nvp(ar, "window", *(settings.size));
#endif
	make_optional_nvp(ar, "pluginPath", settings.pluginPath);
	make_optional_nvp(ar, "vmmode", settings.vmmode);
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
