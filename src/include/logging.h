#pragma once
#include "config.h"
#if USE_LOGGING
#include <typeinfo>
#include "str_util.h"
String demangleName(String toDemangle);
template <class T>
String typeName(const T& t) {
    return demangleName(typeid(t).name());
}
void global_log_format_impl(const char *file, int line, const char *func, const char *fmt, ...);
#define OBJ(x) typeid(x).name()
#define my_printf(fmt, ...) global_log_format_impl(__FILE__, __LINE__, __FUNCTION__, fmt, __VA_ARGS__)
#define log_out(fmt, ...) global_log_format_impl(nullptr, 0, nullptr, fmt, __VA_ARGS__)
#define log_printf my_printf
void logEveryMsec(int32_t nId, int32_t delayMs, String str);
#else
#define my_printf(fmt, ...) 
#define LOG_EVERY_MSEC(delay, str)
#define OBJ(x)
#endif

class ThreadSafeFileLogger;
class Logger {
public:
	virtual ~Logger() { }
	virtual void log(const char* data, size_t len) = 0;
	virtual void logStr(String s) = 0;
};

Logger* getGlobalLogger();
