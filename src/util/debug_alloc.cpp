#include "str_util.h"

#include <vector>
#include "../gui/gui.h"
#include "logging.h"

int allocCount = 0;
std::vector<guibase*>* g_guis = new std::vector<guibase*>();

void printLeaked() {
	my_printf("allocCount %d\n", allocCount);
	for (auto it = g_guis->begin(); it != g_guis->end(); it++) {
		guibase* ctrl = *it;
		my_printf("leaked %d %s\n", ctrl->id, StringAsCStr(ctrl->getClassName()));
	}
	g_guis->clear();
	g_guis->shrink_to_fit();
}
