#include <vector>
#include "str_util.h"
#include "host/main_daw.h"
#include "util/testing_environment.h"

int main(int, char*[]) {
    daw_test::currentTest = daw_test::TestCases::TEST_HOST_EXCEPTIONS;
    std::vector<String> args{
        "-f", "cpp-test-data/serum-test.project",
        "-s", "32.0",
        "-l", "4.0",
        "-o", "test-render",
        "--render"
    };
    int ret = HostCLI::runCommandLineHost(args);
    return (ret == 1) ? 0 : 1;
}
