#include <host/main_daw.h>
#include "mainctrl.h"
#include <memory>

void deleteApp() {
}

std::shared_ptr<AppCtrl> makeApp(std::vector<String>& args) {
    return nullptr;
}
void startApp(std::shared_ptr<AppCtrl>& app) {
}


int main(int argc, char* argv[]) {
    return runCommandLineHost(argc, (const char**) argv);
}
