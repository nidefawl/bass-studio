#pragma once
#include <vector>
#include "config.h"
#include "theme.h"

struct themefile {
public:
    guitheme_t theme;
    themefile() = default;
};
themefile loadTheme(const String& path);
void saveTheme(const String& path, themefile& _settings);
