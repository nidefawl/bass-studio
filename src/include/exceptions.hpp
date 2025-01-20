#pragma once
#include <stdexcept>
#include "str_util.hpp"
#include "assert_dbg.h"

String FormatErrorMessage(uint32_t error, const String& msg = "");
class FileIOException final : public std::runtime_error {
private:
    int32_t m_error;

public:
    FileIOException(uint32_t error, const String& msg)
        : std::runtime_error(FormatErrorMessage(error, msg)), m_error((int32_t) error) {}
    explicit FileIOException(const String& msg)
        : std::runtime_error(msg), m_error(1) {}

    int32_t GetErrorCode() const { return m_error; }
};
class SystemException final : public std::runtime_error {
private:
    int32_t m_error;

public:
    SystemException(uint32_t error, const String& msg)
        : std::runtime_error(FormatErrorMessage(error, msg)), m_error((int32_t) error) {}

    int32_t GetErrorCode() const { return m_error; }
};

class appexception final : public std::runtime_error {
public:
    explicit appexception(const String& str) : runtime_error(str) {
    }
};

class applogicexception final : public std::runtime_error {
public:
    explicit applogicexception(const String& str) : runtime_error(str) {
        dbgassert(0);
    }
    explicit applogicexception(const char* msg) : runtime_error(msg) {
        dbgassert(0);
    }
};
