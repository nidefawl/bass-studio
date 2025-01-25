#pragma once
#include "projectfile.hpp"

namespace DAW::ProjectFileV1 {

std::optional<String> saveProject(const std::shared_ptr<project_file>& f, std::vector<uint8_t>& bufferOut);
std::variant<std::shared_ptr<project_file>, String> loadProject(const std::vector<uint8_t>& vec);

std::optional<String> saveProjectToJsonFile(const std::shared_ptr<project_file>& f, const String& path);
std::variant<std::shared_ptr<project_file>, String> loadProjectFromJsonFile(const String& path);

std::optional<String> saveTrackContainer(const trackcontainer_snapshot_t& container, const String& path);
std::variant<std::shared_ptr<trackcontainer_snapshot_t>, String> loadTrackContainer(const String& path);

std::optional<String> serializePluginSnapshot(const plugin_snapshot_t& snapshot, std::vector<uint8_t>& buf);
std::variant<std::shared_ptr<plugin_snapshot_t>, String> deserializePluginSnapshot(std::vector<uint8_t>& vec);

} // namespace DAW::ProjectFileV1
