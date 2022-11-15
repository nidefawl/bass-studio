#include "basectrl.h"
#include "host/daw/mainctrl.h"
#include "str_util.h"
#include <memory>
#include <vector>

class DawAppInstService final : public AppInstanceService {
    std::shared_ptr<MainCtrl> mainctrl;
    std::shared_ptr<DawInstance> dawInstance;
public:
    ~DawAppInstService() override = default;
    std::shared_ptr<AppCtrl> makeApp(const std::vector<String>& args) override {
        dbgassert (!mainctrl);
        dawInstance = std::make_shared<DawInstance>();
        dawInstance->initDaw();
        mainctrl = std::make_shared<MainCtrl>(*dawInstance);
        mainctrl->initApp(args);
        dawInstance->setMainControl(mainctrl.get());

        return mainctrl;
    }

    void startApp(std::shared_ptr<AppCtrl>& app) override {
        dawInstance->startDaw();
        dawInstance->initProcessingResources();
        dawInstance->initRealtimeResources();
        app->startApp();
    }

    void deleteApp() override {
        mainctrl = nullptr;
        dawInstance = nullptr;
    }
};

int main(int argc, char* argv[]) {
    DawAppInstService instService;
    std::vector<String> vecArgs(&argv[0], &argv[argc]);
    vecArgs.insert(vecArgs.end(), {"--logfile", String(BuildInfo::PRODUCT_NAME_LOWER) + ".log"});
    int retVal = startApplication(vecArgs, instService);
    return retVal;
}
