#pragma once
#include <stdexcept>
#include "str_util.h"


String FormatErrorMessage(int32_t error, String msg = "");
class FileIOException : public std::runtime_error
{
private:
	int32_t m_error;
public:
	FileIOException(int32_t error, const String& msg)
		: std::runtime_error(FormatErrorMessage(error, msg)), m_error(error) { }

	int32_t GetErrorCode() const { return m_error; }
};
class SystemException : public std::runtime_error
{
private:
	int32_t m_error;
public:
	SystemException(int32_t error, const String& msg)
		: std::runtime_error(FormatErrorMessage(error, msg)), m_error(error) { }

	int32_t GetErrorCode() const { return m_error; }
};

class appexception : public std::runtime_error {
public:
	appexception(const char* msg) : runtime_error(msg) {}
};

class applogicexception : public std::runtime_error {
public:
	applogicexception(String str) : runtime_error(str) {}
	applogicexception(const char* msg) : runtime_error(msg) {}
};
