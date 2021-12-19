#include <memory>
#include <sstream>
#include <iostream>
#include <vector>
#include "str_util.h"
#include "host/main_daw.h"
#include "tests/test_environment.h"

int main(int argc, char* argv[]) {
	daw_test::currentTest = daw_test::TestCases::TEST_HOST_EXCEPTIONS;
	std::vector<String> vecStringArgs {
		"-f", "cpp-test-data/serum-test.project",
		"-s", "32.0",
		"-l", "12.0",
		"-o", "test-render",
		"--render"
	};
	std::vector<const char*> vecArgs(vecStringArgs.size());
	std::transform(vecStringArgs.begin(),
	               vecStringArgs.end(),
				   vecArgs.begin(),
	               [](String& param) { return param.c_str(); });
	return runCommandLineHost((int) vecArgs.size(), vecArgs.data());
}
