#pragma once
#include "config.h"
#include "logging.h"
#include <string_view>
#if USE_LOGGING
#include <typeinfo>
#include "str_util.h"
#include "util/nameoftype.hpp"

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
        LEVEL_TRACE = 0,
        LEVEL_DEBUG,
        LEVEL_INFO,
        LEVEL_WARN,
        LEVEL_ERROR,
        LEVEL_FATAL
    };
    void log_filtered(std::string_view id, Level lvl, const char* file, int line, const char* func, const char* fmt, ...) noexcept;
}

#define log_f(lvl, fmt, ...) ::Log::log_filtered(NAMEOFOBJ(*this), lvl, __FILE__, __LINE__, __FUNCTION__, fmt, __VA_ARGS__)

Logger* getGlobalLogger() noexcept;

void log_format_to_logger(Logger* logger, const char* file, int line, const char* func, const char* fmt, ...) noexcept;
#define log_printf(fmt, ...) log_format_to_logger(getGlobalLogger(), __FILE__, __LINE__, __FUNCTION__, fmt, __VA_ARGS__)
#define log_out(fmt, ...) log_format_to_logger(getGlobalLogger(), nullptr, 0, nullptr, fmt, __VA_ARGS__)
#else
#define log_printf(fmt, ...)
#endif
