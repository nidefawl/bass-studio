#pragma once
#include "theme.h"
#include "str_util.h"
#include <vector>

class BaseCtrl;
class guitheme_mgr {
private:
    friend class BaseCtrl;
    BaseCtrl* parent = nullptr;
    guitheme_t defaultTheme;
    guitheme_t current;
    std::vector<guitheme_t> themes;
    guitheme_mgr() = default;

public:
    guitheme_t& getRef() {
        return current;
    }
    String getThemeName() const {
        return current.name;
    }
    void loadThemes();
    void getThemes(std::vector<guitheme_t>& _out);
    void getThemeNames(std::vector<String>& _out);
    void setThemeName(const String& themeName);
    void setTheme(guitheme_t theme);
    void cloneCurrentTheme(const String& themeName);
    void removeCurrentTheme();
    void saveCurrentTheme();
};
