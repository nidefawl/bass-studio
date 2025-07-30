#pragma once
#include "host/plugin/modules.hpp"
#include "str_util.hpp"
#include <map>
#include <memory>
#include <optional>

namespace DAW::Host::PluginScanner {

struct pluginscanner_server_options {
    std::map<ModuleType, std::vector<String>> pluginPathLists;
    bool dryRun             = false;
    bool fullRescan         = false;
    bool launchProcess      = true;
    bool checkDiskTimestamp = true;
    String updatePattern;
    int32_t unresponsiveTimeoutSeconds = 120;
};

class PluginScannerServer {
    class Impl;
    std::shared_ptr<Impl> impl;
public:
    PluginScannerServer(const pluginscanner_server_options& options, std::optional<String> pluginscannerExecPath);

    void findFiles();

    int runScannerServer();
};


} // namespace DAW::Host::PluginScanner