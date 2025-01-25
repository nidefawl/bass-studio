#pragma once
#include "config.hpp"
#include "host/track/track.hpp"
#include <memory>
#include "host/audiocache/samplefileidx.hpp"
#include "snapshot/project-snapshot.hpp"
#include <optional>
#include <variant>

struct project_file {
    uint32_t fileFmtVersion = 0;
    String path;
    project_snapshot_t project;
    project_layout_t layout;
    samplefile_index_t sampleFileIndex;
    std::vector<dawview_layout_t> layouts;
};

struct project_to_load_t {
    std::shared_ptr<project_file> projectfile;
    int loadflags;
};
