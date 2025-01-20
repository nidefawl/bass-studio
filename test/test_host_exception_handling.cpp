#include <vector>
#include "TestBase.hpp"
#include "str_util.hpp"
#include "host/daw/main_daw.hpp"
#include "util/testing_environment.hpp"

int main(int, char*[]) {
    daw_test::currentTest = daw_test::TestCases::TEST_HOST_EXCEPTIONS;
    std::vector<String> args{
        "-f", TEST_PATH("projects/test-vst.project"),
        "-s", "32.0",
        "-l", "4.0",
        "-o", "test-render",
        "--render"
    };
    int ret = HostCLI::runCommandLineHost(args);
    return (ret == 1) ? 0 : 1;
}
