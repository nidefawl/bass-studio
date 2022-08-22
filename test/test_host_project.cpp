#include "str_util.h"
#include "host/main_daw.h"
#include "host/mainctrl.h"
#include <memory>
#include <sstream>
#include <iostream>
#include <vector>

int main(int, char*[]) {
    std::vector<String> args{
        "-f", "cpp-test-data/test-vst.project",
        "-s", "1137.0",
        "-l", "24.0",
        "-o", "test-render",
        "--render"
    };
    return HostCLI::runCommandLineHost(args);
}
