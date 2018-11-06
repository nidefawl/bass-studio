#pragma once
#ifndef __cplusplus
#include <stdarg.h>
#else
#include <cstdarg>
extern "C"
{
#endif

#define insane_free(ptr) { free(ptr); ptr = 0; }

	int _________vasprintf(char **strp, const char *fmt, va_list ap);
	int _________asprintf(char **strp, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#ifdef __GNUC__
#include <string.h>
#ifndef _T
#define _T
#endif
#endif
#ifdef _MSC_VER
#ifdef UNICODE
#define USE_WSTRING
#endif
#include <tchar.h>
#endif

#ifdef __linux__
#define _snprintf_s(a,b,c,...) snprintf(a,b,__VA_ARGS__)
#endif

#include <string>
#include <algorithm>
#include <locale>
#ifdef USE_WSTRING
#define TX L
using String = std::wstring;
using Stringstream = std::wstringstream;
#else
using String = std::string;
using Stringstream = std::stringstream;
#endif
#define StringAsCStr(x) ((x).c_str())
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

// templated version of my_equal so it could work with both char and wchar_t
template<typename charT>
struct my_equal {
    my_equal( const std::locale& loc ) : loc_(loc) {}
    bool operator()(charT ch1, charT ch2) {
        return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
    }
private:
    const std::locale& loc_;
};

// find substring (case insensitive)
template<typename T>
int ci_find_substr( const T& str1, const T& str2, const std::locale& loc = std::locale() )
{
    typename T::const_iterator it = std::search( str1.begin(), str1.end(),
        str2.begin(), str2.end(), my_equal<typename T::value_type>(loc) );
    if ( it != str1.end() ) return it - str1.begin();
    else return -1; // not found
}

std::wstring s2ws(const std::string& s);
