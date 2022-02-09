#include "mainctrl.h"
#include "str_util.h"
#include <memory>
#include <vector>

int startApplication(const std::vector<String>& args);

std::shared_ptr<MainCtrl> mainctrl;
std::shared_ptr<DawInstance> dawInstance;
std::shared_ptr<AppCtrl> makeApp(const std::vector<String>& args) {
    dbgassert (!mainctrl);


    dawInstance = std::make_shared<DawInstance>();
    dawInstance->initDaw(args);
    mainctrl = std::make_shared<MainCtrl>(*dawInstance);
    mainctrl->initApp(args);
    dawInstance->setMainControl(mainctrl.get());

    return mainctrl;
}

void startApp(std::shared_ptr<AppCtrl>& app) {
    dawInstance->startDaw();
    app->startApp();
    dawInstance->postInit();
}

void deleteApp() {
    mainctrl = nullptr;
    dawInstance = nullptr;
}

int main(int argc, char* argv[]) {
    std::vector<String> vecArgs(&argv[0], &argv[argc]);
    vecArgs.insert(vecArgs.end(), {"--log", "daw.log"});
    int retVal = startApplication(vecArgs);
    /**
     * manually end lifetime here before the at-exit destructors for static objects runs.
     * the destructors have assertions that might abort/print stacktraces that
     * might not work as intended if triggered after main function returns
     */
    mainctrl.reset();
    dawInstance.reset();
    return retVal;
}
