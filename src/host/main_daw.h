#pragma once
#include "basectrl.h"
#include "str_util.h"
#include <vector>

namespace HostCLI {
int runCommandLineHost(const std::vector<String>& args);
}