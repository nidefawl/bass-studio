#include <vector>
#include "logging.h"
#include "thememgr.h"

#include "str_util.h"
#include "theme.h"
#include "themefile.h"
#include "basectrl.h"
#include "platform.h"

void saveThemeFile(themefile& _settings);
themefile loadThemeFile();

bool hasThemeWithName(const std::vector<guitheme_t>& themes, const String& themeName) {
    auto it = std::find_if(begin(themes), end(themes), [themeName](guitheme_t const& x) { return x.name == themeName; });
    return it != end(themes);
}

void guitheme_mgr::saveCurrentAsNewTheme(String name) {
    int32_t idx     = 0;
    String baseName = name;
    String themeName;
    do {
        themeName = baseName;
        if (idx > 0) {
            themeName = baseName + StringFormat(" %d", idx);
        }
        idx++;
    } while (hasThemeWithName(themes, themeName));
    guitheme_t copy = current;
    copy.isDefault  = false;
    copy.name       = themeName;
    themes.push_back(copy);
    this->setThemeName(themeName);
}

void guitheme_mgr::saveThemes() {

    for (auto& t : themes) {
        if (!t.isDefault && t.name == current.name) {
            t = current;
        }
    }
    themefile themeFile;
    themeFile.defaultTheme = defaultTheme;
    themeFile.theme        = current;
    themeFile.themes       = themes;
    auto it                = themeFile.themes.begin();
    while (it != themeFile.themes.end()) {
        if (!it->name.length() || it->name == "default" || it->isDefault) {
            it = themeFile.themes.erase(it);
        } else {
            ++it;
        }
    }
    try {
        saveThemeFile(themeFile);
    } catch (std::exception& e) {
        log_lf(Log::L_ERROR, "Failed saving theme file %s: %s\n", StringAsCStr(App::Platform::toUserdataPath(THEMEFILE_NAME)), e.what());
    }
}

void guitheme_mgr::loadThemes() {
    guitheme_t theme;
    theme.isDefault = true;
    theme.name      = "default";
    defaultTheme    = theme;
    if (current.name.empty()) {
        current.name = "default";
    }
    themefile themeFile;
    try {
        themeFile = loadThemeFile();
    } catch (std::exception& e) {
        log_lf(Log::L_DEBUG, "Using internal theme: %s\n", e.what());
    }
    String selectedTheme   = themeFile.theme.name;
    auto it                = themeFile.themes.begin();
    while (it != themeFile.themes.end()) {
        it->isDefault = false;
        if (!it->name.length() || it->name == "default") {
            it = themeFile.themes.erase(it);
        } else {
            ++it;
        }
    }
    themeFile.themes.insert(themeFile.themes.begin(), defaultTheme);
    themes = themeFile.themes;
    setThemeName(selectedTheme);
}
void guitheme_mgr::removeTheme(guitheme_t theme) {
    removeThemeName(theme.name);
}
void guitheme_mgr::setTheme(guitheme_t setTheme) {
    for (guitheme_t& theme2 : themes) {
        // save current theme
        if (!theme2.isDefault && theme2.name == current.name) {
            theme2 = current;
        }
    }
    if (setTheme.isDefault) {
        setTheme = defaultTheme;
    }
    current = setTheme;
    current.bindFonts();
    if (parent && parent->isOk()) {
        parent->relayout();
    }
}

void guitheme_mgr::setThemeName(String themeName) {
    if (themeName == "default") {
        setTheme(defaultTheme);
        return;
    }
    auto it = std::find_if(begin(themes), end(themes), [themeName](guitheme_t const& x) { return x.name == themeName; });
    if (it != end(themes)) {
        guitheme_t& themeByName = *it;
        setTheme(themeByName);
    } else {
        setTheme(defaultTheme);
    }
}

void guitheme_mgr::removeThemeName(String themeName) {
    String nextTheme = "";
    bool erased      = false;
    auto it          = themes.begin();
    while (it != themes.end()) {
        if (!it->isDefault && (!it->name.length() || it->name == themeName)) {
            it     = themes.erase(it);
            erased = true;
        } else {
            if (!erased) {
                nextTheme = it->name;
            }
            ++it;
        }
    }
    if (erased) {
        if (nextTheme.empty()) {
            setTheme(defaultTheme);
        } else {
            setThemeName(nextTheme);
        }
    }
}

void guitheme_mgr::getThemes(std::vector<guitheme_t>& _out) {
    _out = this->themes;
}

void guitheme_mgr::getThemeNames(std::vector<String>& _out) {
    for (guitheme_t& th : this->themes) {
        _out.push_back(th.name);
    }
}
