#include "host/main_daw.h"
#include "mainctrl.h"
#include <memory>

int main(int argc, char* argv[]) {
    std::vector<String> vecArgs(&argv[0], &argv[argc]);
    vecArgs.insert(vecArgs.end(), {"--logfile", "daw-cli.log"});
    return HostCLI::runCommandLineHost(vecArgs);
}
