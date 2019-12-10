#include "mainctrl.h"

int startApplication(int argc, char* argv[]);

std::shared_ptr<MainCtrl> mainctrl;
std::shared_ptr<AppCtrl> makeApp() {
	mainctrl = std::make_shared<MainCtrl>();
	return mainctrl;
}


void deleteApp() {
	mainctrl = nullptr;
}
int main(int argc, char* argv[]) {
	return startApplication(argc, argv);
}
