#pragma once
#ifndef __cplusplus
#include <stdarg.h>
#else
#include <cstdarg>
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#ifdef __GNUC__
#include <cstring>
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
#define _snprintf_s(a, b, c, ...) snprintf(a, b, __VA_ARGS__)
#endif

#include <string>
#include <algorithm>
#include <locale>
#ifdef USE_WSTRING
#define TX L
using String       = std::wstring;
using Stringstream = std::wstringstream;
#else
using String       = std::string;
using Stringstream = std::stringstream;
#endif
#define StringAsCStr(x) ((x).c_str())

#if defined(_MSC_VER)
# define FORMAT(f, a)
#else  // _MSC_VER
# define FORMAT(f, a)  __attribute__((format(printf, f, a)))
#endif  // _MSC_VER

String StringFormat(const char* fmt, ...) FORMAT(1, 2);
String FormatTempo(float tempo);
String StringLimit(String s, int limit);
void replaceString(String& s, String f, String r);
const char* noteName(int note);//DONT KEEP REFERENCE
namespace StrUtil {
int32_t StringReplace(String& s, const String& f, const String& r);
}

template<typename T>
String FormatBinaryString(T i) {
    static const char* bit_rep[16] = {
        "0000", "0001", "0010", "0011",
        "0100", "0101", "0110", "0111",
        "1000", "1001", "1010", "1011",
        "1100", "1101", "1110", "1111",
    };
    std::string str;
    int nibbles = sizeof(T) * 2;
    while (nibbles-- > 0) {
        str += bit_rep[((i >> (nibbles * 4)) & 0xF)];
    }
    return str;
}
inline bool StrEndsWith(String const& a, String const& b) {
    if (b.size() > a.size()) return false;
    return std::equal(a.begin() + static_cast<uint32_t>(a.size() - b.size()), a.end(), b.begin());
}
inline bool StrStartsWith(String const& a, String const& b) {
    if (b.size() > a.size()) return false;
    return std::equal(a.begin(), a.begin() + static_cast<uint32_t>(b.size()), b.begin());
}
inline String StringTrim(String str) {
    // trim trailing spaces
    size_t endpos   = str.find_last_not_of(" \t\n\r");
    size_t startpos = str.find_first_not_of(" \t\n\r");
    if (std::string::npos != endpos) {
        str = str.substr(0, endpos + 1);
        str = str.substr(startpos);
        return str;
    }
    if (std::string::npos != startpos) {
        str = str.substr(startpos);
    }
    return str;
}
inline String StringToUpper(String strToConvert) {
    std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(), ::toupper);
    return strToConvert;
}

int StringContainsCI(const String& str1, const String& str2, const std::locale& loc = std::locale());
void replaceBackslashInString(String& str);

/**
 * Replace backslash with forward slash and write output to buf.
 * If the output buffer is too small the string will be truncated and no error is signaled
 *
 * @param filename input string
 * @param buf buffer to write the replaced string to
 * @param bufOutSize size of buf
 */
void replaceBackslashWithForwardslash(const char* filename, char* buf, size_t bufOutSize);
const char* relFileName(const char* input);
const char* removeLeadingPathSegments(const char* input, int maxPathSegs = 1);

template<class T, size_t N, class S>
void safe_strcpy(T (&dest)[N], S src, size_t len) {
    static_assert(N > 0, "safe_strcpy: buffer cannot be size 0");
    memcpy(dest, src, len > N - 1 ? N - 1 : len);
    dest[len > N - 1 ? N - 1 : len] = 0;
}

template<class T, size_t N>
void safe_strcpy(T (&dest)[N], const String& str) {
    static_assert(N > 0, "safe_strcpy: buffer cannot be size 0");
    auto len = str.length();
    memcpy(dest, StringAsCStr(str), len > N - 1 ? N - 1 : len);
    dest[len > N - 1 ? N - 1 : len] = 0;
}

template<class T, size_t N>
void safe_strcpy(T (&dest)[N], const char* src) {
    static_assert(N > 0, "safe_strcpy: buffer cannot be size 0");
    const auto len = strnlen(src, N-1);
    memcpy(dest, src, len > N - 1 ? N - 1 : len);
    dest[len > N - 1 ? N - 1 : len] = 0;
}

inline void safe_str_to_buf(char* dest, size_t destLen, const String& str) {
    auto len = str.length();
    memcpy(dest, StringAsCStr(str), len > destLen - 1 ? destLen - 1 : len);
    dest[len > destLen - 1 ? destLen - 1 : len] = 0;
}
