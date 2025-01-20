#pragma once
#include <vector>
#include "config.hpp"
#include "theme.hpp"

struct themefile {
public:
    guitheme_t theme;
    themefile() = default;
};
themefile loadTheme(const String& path);
bool saveTheme(const String& path, themefile& _settings);
