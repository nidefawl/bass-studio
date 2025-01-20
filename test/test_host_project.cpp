#include "TestBase.hpp"
#include "str_util.hpp"
#include "host/daw/main_daw.hpp"
#include "host/daw/mainctrl.hpp"
#include <memory>
#include <sstream>
#include <iostream>
#include <vector>

int main(int, char*[]) {
    std::vector<String> args{
        "-f", TEST_PATH("projects/test-vst.project"),
        "-o", "test-render",
        "--render"
    };
    return HostCLI::runCommandLineHost(args);
}
