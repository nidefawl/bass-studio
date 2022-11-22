#include <vector>
#include "TestBase.hpp"
#include "str_util.h"
#include "host/daw/main_daw.h"
#include "util/testing_environment.h"

int main(int, char*[]) {
    daw_test::currentTest = daw_test::TestCases::TEST_HOST_EXCEPTIONS;
    std::vector<String> args{
        "-f", TEST_PATH("projects/serum-test.project"),
        "-s", "32.0",
        "-l", "4.0",
        "-o", "test-render",
        "--render"
    };
    return HostCLI::runCommandLineHost(args);
}
