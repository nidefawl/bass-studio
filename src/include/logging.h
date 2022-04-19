#pragma once
#include <string_view>
#include <vector>
#include <typeinfo>
#include "config.h"
#include "str_util.h"

#ifndef ENABLE_LOGGING
#define ENABLE_LOGGING 1
#endif


String demangleName(const char* toDemangle);
template<class T>
String typeName(const T& t) {
    return demangleName(typeid(t).name());
}
class Logger;
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
    void log_fmt(Logger* logger, Level lvl, const char* file, int line, const char* func, const char* fmt, ...) noexcept FORMAT(6, 7);
}
class Logger {
    Log::Level lvl = Log::LEVEL_ALL;
public:
    Logger() noexcept = default;
    virtual ~Logger() = default;
    virtual void log(Log::Level lvl, const char* data, size_t len) = 0;
    virtual void logStr(Log::Level lvl, String s)                  = 0;
    virtual void setLevel(Log::Level lvl) noexcept {
        this->lvl = lvl;
    }
    virtual Log::Level getLevel() {
        return this->lvl;
    }
};

class MultiLogger : public Logger {
    std::vector<Logger*> loggers;
public:
    explicit MultiLogger(Logger* handle = nullptr) noexcept {
        if (handle)
            loggers.push_back(handle);
    }
    void addLogger(Logger* _logger) {
        loggers.push_back(_logger);
    }
    void removeLogger(Logger* _logger) {
        loggers.erase(std::remove(loggers.begin(), loggers.end(), _logger), loggers.end());
    }
    ~MultiLogger() override = default;
    void log(Log::Level lvl, const char* data, size_t len) override {
        for (auto* logger : loggers) {
            logger->log(lvl, data, len);
        }
    }
    void logStr(Log::Level lvl, String s) override {
        for (auto* logger : loggers) {
            logger->logStr(lvl, s);
        }
    }
};
MultiLogger& getMultiLogger() noexcept;
Logger* getGlobalLogger() noexcept;


#if ENABLE_LOGGING
#define log_to(logger, lvl, fmt, ...) ::Log::log_fmt(logger, lvl, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define log_lf(lvl, fmt, ...) log_to(getGlobalLogger(), lvl, fmt, ##__VA_ARGS__)
#define log_printf(fmt, ...) log_lf(::Log::L_INFO, fmt, ##__VA_ARGS__)
#define log_out(fmt, ...) ::Log::log_fmt(getGlobalLogger(), ::Log::L_INFO, nullptr, 0, nullptr, fmt, ##__VA_ARGS__)
#else
#define log_to(logger, lvl, fmt, ...)
#define log_lf(lvl, fmt, ...)
#define log_printf(fmt, ...)
#define log_out(fmt, ...)
#endif
