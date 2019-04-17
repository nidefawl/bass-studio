#pragma once
#include "config.h"
#include "track.h"
#include <memory>
#include "samplefileidx.h"

class MainCtrl;
struct project_file {
	String path;
	project_snapshot_t project;
	project_layout_t layout;
	samplefile_index_t sampleFileIndex;
};
bool saveProject(std::shared_ptr<project_file> f, String& path);
std::shared_ptr<project_file> loadProjectFile(String& path);
