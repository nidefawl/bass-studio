#pragma once
#include <vector>
#include "config.h"
#include "theme.h"

struct themefile
{
public:
	guitheme_t theme;
	std::vector<guitheme_t> themes;
	themefile() { }
};
void saveThemeFile(themefile& _settings);
bool loadThemeFile(themefile& _settings);


