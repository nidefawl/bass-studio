#pragma once
#ifndef __cplusplus
#include <stdarg.h>
#else
#include <cstdarg>
extern "C"
{
#endif

#define insane_free(ptr) { free(ptr); ptr = 0; }

	int vasprintf(char **strp, const char *fmt, va_list ap);
	int asprintf(char **strp, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define USE_WSTRING
#endif

#ifdef __MINGW32__
#include <string.h>
#define _T
#else
#include <tchar.h>
#endif

#include <string>
#include <algorithm>
#ifdef USE_WSTRING
#define TX L
using String = std::wstring;
using Stringstream = std::wstringstream;
#else
using String = std::string;
using Stringstream = std::stringstream;
#endif
#define StringAsCStr(x) (x.c_str())
String StringFormat(const char *fmt, ...);
String FormatTempo(float tempo);
String StringLimit(String s, int limit);
void replaceString(String& s, String f, String r);
const char* noteName(int note); //NOT THREAD SAFE; DONT KEEP REFERENCE
inline bool StrEndsWith(String const & a, String const & b)
{
    if (b.size() > a.size()) return false;
    return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
}
inline String StringToUpper(String strToConvert)
{
    std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(), ::toupper);

    return strToConvert;
}
std::wstring s2ws(const std::string& s);
