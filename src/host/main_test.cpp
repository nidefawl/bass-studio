#include "basectrl.h"
int startApplication(int argc, char* argv[]);

std::shared_ptr<AppCtrl> makeApp() {
	return nullptr;
}

void deleteApp() {
}
int main(int argc, char* argv[]) {
	return startApplication(argc, argv);
}
