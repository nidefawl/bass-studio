#pragma once
#include "fileio.h"
#include "str_util.h"
#include <algorithm>
#include <vector>

class PresetManager {
public:
    struct Preset {
        String name;
        String path;
        bool isFavorite = false;
    };

private:
    String fileExtension = "preset";
    String presetPath;
    std::vector<Preset> presets;
    std::vector<Preset> favorites;

public:
    const String& getPresetPath() const {
        return presetPath;
    }
    const String& getFileExtension() const {
        return fileExtension;
    }
    void setFileExtension(const String& ext) {
        fileExtension = ext;
    }

    void load(const String& path) {
        presetPath = path;
        presets.clear();
        favorites.clear();

        std::vector<FileFound> files;
        findFilesWithExt(path, fileExtension, true, files);
        for (const auto& file : files) {
            Preset preset;
            preset.name = file.name;
            preset.path = file.path;
            presets.push_back(preset);
        }
        std::stable_sort(presets.begin(), presets.end(), [](const Preset& a, const Preset& b) {
            return a.path < b.path;
        });
    }
    void reload() {
        load(presetPath);
    }
    const std::vector<Preset>& getPresets() const {
        return presets;
    }
    const std::vector<Preset>& getFavorites() const {
        return favorites;
    }
};