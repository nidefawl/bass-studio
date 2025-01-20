#pragma once
#include "basectrl.hpp"
#include "str_util.hpp"
#include <vector>

namespace HostCLI {
int runCommandLineHost(const std::vector<String>& args);
}