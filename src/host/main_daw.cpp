#include "mainctrl.h"
#include "str_util.h"
#include <memory>
#include <vector>

int startApplication(int argc, char* argv[]);

std::shared_ptr<MainCtrl> mainctrl;
std::shared_ptr<DawInstance> dawInstance;
std::shared_ptr<AppCtrl> makeApp() {
    if (!mainctrl) {
        dawInstance = std::make_shared<DawInstance>();
        DawInstance& dawRef = *dawInstance;
        mainctrl = std::make_shared<MainCtrl>(dawRef);
        std::vector<std::shared_ptr<AppCtrl>> companions;
        dawRef.setMainControl(mainctrl.get());
    }
    return mainctrl;
}

void deleteApp() {
    mainctrl = nullptr;
}

inline char* StringAsMStr(String& str) {
    return str.empty() ? nullptr : &*str.begin();
}

int main(int argc, char* argv[]) {
    std::vector<char*> vecArgs(&argv[0], &argv[argc]);
    String strExtraArgs[] = {"-log", "daw.log"};
    vecArgs.push_back(StringAsMStr(strExtraArgs[0]));
    vecArgs.push_back(StringAsMStr(strExtraArgs[1]));
    int retVal = startApplication(static_cast<int>(vecArgs.size()), vecArgs.data());
    /**
     * manually end lifetime here before the at-exit destructors for static objects runs.
     * the destructors have assertions that might abort/print stacktraces that
     * might not work as intended if triggered after main function returns
     */
    mainctrl.reset();
    dawInstance.reset();
    return retVal;
}
