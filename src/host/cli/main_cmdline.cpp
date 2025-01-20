#include "host/daw/main_daw.hpp"
#include "host/daw/mainctrl.hpp"
#include <memory>

int main(int argc, char* argv[]) {
    std::vector<String> vecArgs(&argv[0], &argv[argc]);
    vecArgs.insert(vecArgs.end(), {"--logfile", "daw-cli.log"});
    return HostCLI::runCommandLineHost(vecArgs);
}
