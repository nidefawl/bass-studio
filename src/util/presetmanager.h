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
    std::vector<Preset> presets;
    std::vector<Preset> favorites;
    std::vector<String> presetPaths;

    void loadPath(const String& path) {
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
public:
    const std::vector<String>& getPresetPaths() const {
        return presetPaths;
    }
    const String& getFileExtension() const {
        return fileExtension;
    }
    void setFileExtension(const String& ext) {
        fileExtension = ext;
    }

    void clear() {
        presets.clear();
        favorites.clear();
    }
    void load(const String& path) {
        presetPaths.push_back(path);
        loadPath(path);
    }

    void reload() {
        clear();
        for (auto& path : presetPaths) {
            loadPath(path);
        }
    }
    const std::vector<Preset>& getPresets() const {
        return presets;
    }
    const std::vector<Preset>& getFavorites() const {
        return favorites;
    }
};