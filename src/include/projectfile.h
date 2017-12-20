#pragma once
#include "config.h"
#include "track.h"
#include <memory>

class MainCtrl;
struct project_file {
	String path;
	project_snapshot_t project;
};
bool saveProject(std::shared_ptr<project_file> f, String& path);
std::shared_ptr<project_file> loadProjectFile(MainCtrl* ctrl, String& path);
