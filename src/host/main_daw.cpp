#include "mainctrl.h"
#include <memory>
#include <vector>

int startApplication(int argc, char* argv[]);

std::shared_ptr<MainCtrl> mainctrl;
std::shared_ptr<DawInstance> dawInstance;
std::shared_ptr<AppCtrl> makeApp() {
	if (!mainctrl) {
		dawInstance = std::make_shared<DawInstance>();
		DawInstance& dawRef = *dawInstance.get();
		mainctrl = std::make_shared<MainCtrl>(dawRef);
		std::vector<std::shared_ptr<AppCtrl>> companions;
		dawRef.setMainControl(mainctrl.get());
	}
	return mainctrl;
}


void deleteApp() {
	mainctrl = nullptr;
}
int main(int argc, char* argv[]) {
	int retVal = startApplication(argc, argv);
	// we manually end lifetime here before the at-exit destructors for static objects runs
	// this is because the destructors have assertions that might throw/abort/print stacktraces that might not work as intended if triggered in at-exit destruction phase
	mainctrl.reset();
	dawInstance.reset();
	return retVal;
}
