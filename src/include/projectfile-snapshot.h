#pragma once
#include "config.h"
#include "str_util.h"
#include "exceptions.h"
#include "logging.h"
#include "snapshot.h"
#include "fileio.h"
#include <sstream>
#include <memory>
#include <vector>
#include <sstream>


extern std::vector<SupportedFileType> vFILE_TYPE_PLUGINSNAPSHOT;

bool savePluginSnapshot(const plugin_snapshot_t& snapshot, const String& path);
std::shared_ptr<plugin_snapshot_t> loadPluginSnapshot(const String& path);
