#include "common/test_appctrl.h"
#include "str_util.h"
#include <memory>
#include <vector>

#include "appconfig.h"
#include "appsettings.h"

class TestAppInstanceService : public AppInstanceService {
    std::shared_ptr<TestAppCtrl> app;

public:
    ~TestAppInstanceService() override = default;

    std::shared_ptr<AppCtrl> makeApp(const std::vector<String>& args) override {
        app = std::make_shared<TestAppCtrl>();
        app->initApp(args);
        return app;
    }

    void startApp(std::shared_ptr<AppCtrl>& app) override {
        app->startApp();
    }

    void deleteApp() override {
        daw_tls::tlsinstance& tls = daw_tls::getTls();
        delete tls.runtime;
        delete tls.settings;
        app = nullptr;
    }
};

int main(int argc, char* argv[]) {
    TestAppInstanceService testAppInstance;
    daw_tls::initNewTls();
    std::vector<String> vecArgs(&argv[0], &argv[argc]);
    vecArgs.insert(vecArgs.end(), { "--log", "gui.log" });
    // vecArgs.insert(vecArgs.end(), { "--center", "1" });
    // vecArgs.insert(vecArgs.end(), { "--app", "5" });
    return startApplication(vecArgs, testAppInstance);
}
