#include <vector>
#include "appsettings.h"
#include "fileio.h"
#include "logging.h"
#include "thememgr.h"

#include "str_util.h"
#include "theme.h"
#include "themefile.h"
#include "basectrl.h"
#include "platform.h"

void guitheme_mgr::cloneCurrentTheme(const String& themeName) {
    String pathTheme = App::Platform::toUserdataPath("themes/" + themeName);
    if (FileExists(pathTheme)) {
        return;
    }
    guitheme_t copy = current;
    copy.isDefault  = false;
    copy.name       = themeName;
    themefile themeFile;
    themeFile.theme = copy;
    saveTheme(pathTheme, themeFile);
    themes.push_back(copy);
}

void guitheme_mgr::removeCurrentTheme() {
    if (current.isDefault)
        return;
    String themeName = current.name;
    String pathTheme = App::Platform::toUserdataPath("themes/" + themeName);
    if (!FileExists(pathTheme)) {
        return;
    }
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
        // delete folder on disk
        DeleteDirectory(pathTheme);
    }
}

void guitheme_mgr::saveCurrentTheme() {
    if (current.isDefault)
        return;
    String pathTheme = App::Platform::toUserdataPath("themes/" + current.name);
    if (!FileExists(pathTheme)) {
        return;
    }
    themefile themeFile;
    themeFile.theme = current;
    saveTheme(pathTheme, themeFile);
}

void guitheme_mgr::loadThemes() {
    guitheme_t theme;
    theme.isDefault = true;
    theme.name      = "default";
    defaultTheme    = theme;
    if (current.name.empty()) {
        auto& settings = daw_tls::getSettings();
        current.name = settings.selectedTheme;
    }
    this->themes.clear();
    themes.push_back(defaultTheme);
    std::vector<FileFound> files;
    try {
        findFilesWithExt(App::Platform::toUserdataPath("themes/"), "json", true, files);
    } catch (std::exception& e) {
        log_lf(Log::L_DEBUG, "Using internal theme: %s\n", e.what());
    }
    String selectedTheme = current.name;
    for (const FileFound& file : files) {
        try {
            String themeParentDir;
            SplitPath(file.path, &themeParentDir, nullptr, nullptr, nullptr);
            themefile themeFile = loadTheme(themeParentDir);
            if (themeFile.theme.name.length()) {
                themes.push_back(themeFile.theme);
            }
        } catch (std::exception& e) {
            log_lf(Log::L_DEBUG, "Failed loading theme %s: %s\n", StringAsCStr(file.path), e.what());
        }
    }
    setThemeName(selectedTheme);
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

void guitheme_mgr::setThemeName(const String& themeName) {
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

void guitheme_mgr::getThemes(std::vector<guitheme_t>& _out) {
    _out = this->themes;
}

void guitheme_mgr::getThemeNames(std::vector<String>& _out) {
    for (guitheme_t& th : this->themes) {
        _out.push_back(th.name);
    }
}
