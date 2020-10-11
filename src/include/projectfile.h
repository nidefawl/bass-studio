#pragma once
#include "config.h"
#include "track.h"
#include <memory>
#include "samplefileidx.h"
#include "project_snapshot.h"

class MainCtrl;
struct project_file {
	String path;
	project_snapshot_t project;
	project_layout_t layout;
	samplefile_index_t sampleFileIndex;
};
bool saveProject(std::shared_ptr<project_file> f, const String& path);
std::shared_ptr<project_file> loadProjectFile(String& path);

bool saveTrackContainer(const trackcontainer_snapshot_t& container, const String& path);
std::shared_ptr<trackcontainer_snapshot_t> loadTrackContainer(const String& path);


