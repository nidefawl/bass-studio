#pragma once
#include "config.h"
#include "track.h"
#include <memory>
#include "samplefileidx.h"
#include "project_snapshot.h"

struct project_file {
    uint32_t fileFmtVersion = 0;
    String path;
    project_snapshot_t project;
    project_layout_t layout;
    samplefile_index_t sampleFileIndex;
    std::vector<dawview_layout_t> layouts;
};

bool saveProject(std::shared_ptr<project_file> f, const String& path);
std::shared_ptr<project_file> loadProjectFile(String& path);

bool saveTrackContainer(const trackcontainer_snapshot_t& container, const String& path);
std::shared_ptr<trackcontainer_snapshot_t> loadTrackContainer(const String& path);
