#include "str_util.h"
#include "host/daw/main_daw.h"
#include "host/daw/mainctrl.h"
#include <memory>
#include <sstream>
#include <iostream>
#include <vector>

int main(int, char*[]) {
    std::vector<String> args{
        "-f", "cpp-test-data/test-vst.project",
        "-o", "test-render",
        "--render"
    };
    return HostCLI::runCommandLineHost(args);
}
