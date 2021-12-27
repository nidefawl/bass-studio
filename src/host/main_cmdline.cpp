#include <host/main_daw.h>
#include "mainctrl.h"
#include <memory>

void deleteApp() {
}

std::shared_ptr<AppCtrl> makeApp() {
    return nullptr;
}


int main(int argc, char* argv[]) {
    return runCommandLineHost(argc, (const char**) argv);
}
