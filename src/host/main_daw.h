#pragma once
#include "str_util.h"
#include <vector>

namespace HostCLI {
int runCommandLineHost(int argc, const char* argv[]);
}

int startApplication(const std::vector<String>& args);
