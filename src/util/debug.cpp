#include "str_util.h"

#ifdef  __MINGW32__

#include <cxxabi.h>

using namespace __cxxabiv1;

String demangleName(String to_demangle)
{
    int status = 0;
    char * buff = __cxxabiv1::__cxa_demangle(to_demangle.c_str(), NULL, NULL, &status);
    String demangled = buff;
    std::free(buff);
    return demangled;
}

#else
String demangleName(String to_demangle)
{
	return to_demangle;
}
#endif
#include <vector>
#include "../gui/gui.h"
#include "logging.h"

int allocCount = 0;
std::vector<guibase*> g_guis;

void printLeaked() {
	my_printf("allocCount %d\n", allocCount);
	for (auto it = g_guis.begin(); it != g_guis.end(); it++) {
		guibase* ctrl = *it;
		my_printf("leaked %d %s\n", ctrl->id, StringAsCStr(ctrl->getClassName()));
	}
}
