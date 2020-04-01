#include "mainctrl.h"
#include <memory>
#include <vector>

int startApplication(int argc, char* argv[]);

std::shared_ptr<MainCtrl> mainctrl;
std::shared_ptr<CompanionCtrl> companion;
std::shared_ptr<DawInstance> dawInstance;
std::vector<std::shared_ptr<AppCtrl>> companions;
std::shared_ptr<AppCtrl> makeApp() {
	if (!mainctrl) {
		dawInstance = std::make_shared<DawInstance>();
		DawInstance& dawRef = *dawInstance.get();
		mainctrl = std::make_shared<MainCtrl>(dawRef);
		companion = std::make_shared<CompanionCtrl>(dawRef);
		dawRef.setControls(mainctrl.get(), companion.get());
//		dawRef.setControls(mainctrl.get(), nullptr);
		companions.push_back(companion);
	}
	return mainctrl;
}
void makeAppCompanions(std::vector<std::shared_ptr<AppCtrl>>& out_Companions) {

	out_Companions = companions;
}


void deleteApp() {
	mainctrl = nullptr;
}
int main(int argc, char* argv[]) {
	return startApplication(argc, argv);
}
