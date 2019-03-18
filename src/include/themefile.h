#pragma once

#include "str_util.h"
#include "color_util.h"
#include "config.h"
#include "theme.h"
#include "msgbox.h"
#include <math.h>
#include <chrono>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>
struct themefile
{
public:
	guitheme_t theme;
	std::vector<guitheme_t> themes;
	themefile() { }
};
void saveThemeFile(themefile& _settings);
bool loadThemeFile(themefile& _settings);


