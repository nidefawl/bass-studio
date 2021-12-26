#pragma once
#include "config.h"
#if USE_LOGGING
#include <typeinfo>
#include "str_util.h"
String demangleName(String toDemangle);
template<class T>
String typeName(const T& t) {
    return demangleName(typeid(t).name());
}
class ThreadSafeFileLogger;
class Logger {
public:
    virtual ~Logger() = default;
    virtual void log(const char* data, size_t len) = 0;
    virtual void logStr(String s)                  = 0;
};

Logger* getGlobalLogger();

void log_format_to_logger(Logger* logger, const char* file, int line, const char* func, const char* fmt, ...);
#define my_printf(fmt, ...) log_format_to_logger(getGlobalLogger(), __FILE__, __LINE__, __FUNCTION__, fmt, __VA_ARGS__)
#define log_out(fmt, ...) log_format_to_logger(getGlobalLogger(), nullptr, 0, nullptr, fmt, __VA_ARGS__)
#define log_printf my_printf
#else
#define my_printf(fmt, ...)
#endif
