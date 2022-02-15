#pragma once
#include "config.h"
#include "logging.h"
#include <string_view>

#ifndef ENABLE_LOGGING
#define ENABLE_LOGGING 1
#endif

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
    Logger() noexcept = default;
    virtual ~Logger() = default;
    virtual void log(const char* data, size_t len) = 0;
    virtual void logStr(String s)                  = 0;
};
namespace Log {
    using Id = int32_t;
    enum Level : int32_t {
        LEVEL_ALL = -1,
        L_TRACE = 0,
        L_DEBUG,
        L_INFO,
        L_WARN,
        L_ERROR,
        L_FATAL
    };
    void log_fmt(Logger* logger, Level lvl, const char* file, int line, const char* func, const char* fmt, ...) noexcept;
}
Logger* getGlobalLogger() noexcept;


#if ENABLE_LOGGING
#define log_to(logger, lvl, fmt, ...) ::Log::log_fmt(logger, lvl, __FILE__, __LINE__, __FUNCTION__, fmt, __VA_ARGS__)
#define log_lf(lvl, fmt, ...) log_to(getGlobalLogger(), lvl, fmt, __VA_ARGS__)
#define log_printf(fmt, ...) log_lf(::Log::L_INFO, fmt, __VA_ARGS__)
#define log_out(fmt, ...) ::Log::log_fmt(getGlobalLogger(), ::Log::L_INFO, nullptr, 0, nullptr, fmt, __VA_ARGS__)
#else
#define log_to(logger, lvl, fmt, ...)
#define log_lf(lvl, fmt, ...)
#define log_printf(fmt, ...)
#define log_out(fmt, ...)
#endif
