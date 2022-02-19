#pragma once
#include "str_util.h"
#include <vector>

namespace HostCLI {
int runCommandLineHost(const std::vector<String>& args);
}

int startApplication(const std::vector<String>& args);
