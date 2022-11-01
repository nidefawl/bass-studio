#pragma once
#include "config.h"
#include "host/track/track.h"
#include <memory>
#include "host/audiocache/samplefileidx.h"
#include "snapshot/project-snapshot.h"

struct project_file {
    uint32_t fileFmtVersion = 0;
    String path;
    project_snapshot_t project;
    project_layout_t layout;
    samplefile_index_t sampleFileIndex;
    std::vector<dawview_layout_t> layouts;
};

bool saveProject(const std::shared_ptr<project_file>& f, std::vector<uint8_t>& bufferOut);
std::shared_ptr<project_file> loadProject(const std::vector<uint8_t>& vec);

bool saveProjectToJsonFile(const std::shared_ptr<project_file>& f, const String& path);
std::shared_ptr<project_file> loadProjectFromJsonFile(const String& path);

bool saveTrackContainer(const trackcontainer_snapshot_t& container, const String& path);
std::shared_ptr<trackcontainer_snapshot_t> loadTrackContainer(const String& path);

std::shared_ptr<plugin_snapshot_t> deserializePluginSnapshot(std::vector<uint8_t>& vec);
bool serializePluginSnapshot(const plugin_snapshot_t& snapshot, std::vector<uint8_t>& buf);