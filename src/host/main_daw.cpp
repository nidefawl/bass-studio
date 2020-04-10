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
	return startApplication(argc, argv);
}
