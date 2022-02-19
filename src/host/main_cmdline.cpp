#include "host/main_daw.h"
#include "mainctrl.h"
#include <memory>

void deleteApp() {
}

std::shared_ptr<AppCtrl> makeApp(const std::vector<String>& args) {
    return nullptr;
}
void startApp(std::shared_ptr<AppCtrl>& app) {
}


int main(int argc, char* argv[]) {
    std::vector<String> vecArgs(&argv[0], &argv[argc]);
    vecArgs.insert(vecArgs.end(), {"--logfile", "daw-cli.log"});
    return HostCLI::runCommandLineHost(vecArgs);
}
