#include "host/main_daw.h"
#include "host/mainctrl.h"
#include <memory>
#include <sstream>
#include <iostream>
#include <vector>

int main(int argc, char* argv[]) {
	std::vector<String> vecStringArgs {
		"-f", "serum-test.project",
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
