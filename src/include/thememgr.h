#pragma once
#include "theme.h"
#include "str_util.h"
#include <vector>
class guitheme_mgr {
	guitheme_t defaultTheme;
	guitheme_t current;
	std::vector<guitheme_t> themes;
public:
	guitheme_t& getRef() {
		return current;
	}
	String getThemeName() {
		return current.name;
	}
	void removeTheme(guitheme_t theme);
	void saveCurrentAsNewTheme(String name);
	void saveThemes();
	void loadThemes();
	void setTheme(guitheme_t theme);
	void getThemes(std::vector<guitheme_t>& _out);
	void getThemeNames(std::vector<String>& _out);
	void removeThemeName(String themeName);
	void setThemeName(String themeName);
};
