#include "str_util.h"

#ifdef __GNUC__

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
	g_guis.clear();
	g_guis.shrink_to_fit();
}

#define MAX_LEN_MY_PRINTF 4096
void _my_printf(const char *file, int line, const char *func, const char *fmt, ...) {
	char buf[MAX_LEN_MY_PRINTF];
	//char buf2[MAX_LEN_MY_PRINTF] = { 0 };
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, MAX_LEN_MY_PRINTF - 1, fmt, args);
	va_end(args);
	const char * pch = !file ? NULL : strrchr(file, '\\');
	pch = pch ? pch+1 : file;
	printf("%s:%d %s: %s", pch, line, func, buf);
	//sprintf_s(buf2, MAX_LEN_MY_PRINTF - 1, "%s:%d %s: %s", file, line, func, buf);
	//appendLog(buf2);
#ifndef _MSC_VER
	fflush(stdout);
#endif
}
