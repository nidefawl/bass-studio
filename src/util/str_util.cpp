#include <array>
#include <climits>
#include <cstdarg>
#include <stdlib.h>
#include <string>
#include <vector>
#include "assert_dbg.h"
#include "math/seq_math.h"
#include "seq_time.h"
#include "str_util.h"

#ifdef _WIN32
#include <windows.h>
#endif

#if __linux__
#include <cstdio>
#endif

String StringFormat(const char* fmt, ...) {
    std::array<char, 4096> FormatBuffer{};
    va_list args;
#ifdef _WIN32
    va_start(args, fmt);
    int ret = vsnprintf_s(FormatBuffer.data(), FormatBuffer.size(), _TRUNCATE, fmt, args);
    va_end(args);
    if (ret == -1) {
        ret = CtrSize(FormatBuffer) - 1;
    }
    if (ret < 0 || ret >= CtrSize(FormatBuffer)) {
        dbgassert(0);
        return {};
    }
#else
    //TODO test truncation on apple
    va_start(args, fmt);
    int ret = vsnprintf(FormatBuffer.data(), FormatBuffer.size(), fmt, args);
    va_end(args);
    if (ret == -1) {
        dbgassert(0);
        return {};
    }
    // linux does the right thing: 
    // write up to FormatBuffer.size()-2 chars and put \0 at FormatBuffer.size()-1
    if (ret >= CtrSize(FormatBuffer)) {
        ret = CtrSize(FormatBuffer);
        dbgassert(FormatBuffer[ret - 1] == '\0');
    }
#endif
    return String{FormatBuffer.data(), static_cast<String::size_type>(ret)};
}

String FormatTempo(float tempo) {
    return StringFormat("%.2f", tempo);
}
String StringLimit(String s, int limit) {
    if (s.length() > (size_t) limit) {
        return s.substr(0, limit);
    }
    return s;
}
void replaceString(String& s, String f, String r) {
    size_t index;
    size_t offset = 0;
    while ((index = s.find(f, offset)) != String::npos) {
        s.replace(index, f.length(), r);
        offset = index + r.length();
    }
}
namespace StrUtil {
int32_t StringReplace(String& s, const String& f, const String& r) {
    int32_t nOccurences = 0;
    size_t index;
    size_t offset = 0;
    while ((index = s.find(f, offset)) != String::npos) {
        s.replace(index, f.length(), r);
        offset = index + r.length();
        nOccurences++;
    }
    return nOccurences;
}
int32_t StringToLower(String& s) {
    int32_t nOccurences = 0;
    for (auto& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c += 32;
            nOccurences++;
        }
    }
    return nOccurences;
}
}

static const char* const noteNames[12]{
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const char* noteName(int note) {//DONT KEEP REFERENCE
    static thread_local std::array<char, 32> FormatBuffer{};
    int noteNameIdx = math::clamp(note % 12, 0, 11);
#ifdef __APPLE__
    int ret = snprintf(FormatBuffer.data(), FormatBuffer.size(), "%s%d", noteNames[noteNameIdx], (note / 12) - 2);
#else
    int ret = _snprintf_s(FormatBuffer.data(), FormatBuffer.size(), _TRUNCATE, "%s%d", noteNames[noteNameIdx], (note / 12) - 2);
#endif
    if (ret < 0) ret = 0;
    FormatBuffer[ret] = 0;
    return FormatBuffer.data();
}
String noteNameAndNumber(int note) {
    int noteNameIdx = math::clamp(note % 12, 0, 11);
    return String(noteNames[noteNameIdx]) + " " + std::to_string((note / 12) - 2) + " (" + std::to_string(note) + ")";
}
#ifdef _WIN32
#ifndef USE_WSTRING
uint32_t wcharToSring(uint32_t codepage, const wchar_t *utf16, size_t utf16_len, std::vector<char>& converted) {
    int len  = WideCharToMultiByte(codepage, 0, utf16, (int)utf16_len, converted.data(), 0, nullptr, nullptr);
    if (len  > 0) {
        converted.reserve(len);
        converted.resize(len);
        converted.front() = 0;
        len  = WideCharToMultiByte(codepage, 0, utf16, (int)utf16_len, converted.data(), (int)converted.size(), nullptr, nullptr);
        if (len > 0) {
            converted.push_back(0);
            return 0;
        }
    }
    return ::GetLastError();
}
#endif

uint32_t stringToWchar(uint32_t codepage, const char* mbsz, size_t mbsz_len, std::vector<wchar_t>& converted) {
    int len  = MultiByteToWideChar(codepage, 0, mbsz, (int)mbsz_len, converted.data(), 0);
    if (len  > 0) {
        converted.reserve(len);
        converted.resize(len);
        converted.front() = 0;
        len  = MultiByteToWideChar(codepage, 0, mbsz, (int)mbsz_len, converted.data(), (int)converted.size());
        if (len > 0) {
            converted.push_back(0);
            return 0;
        }
    }
    return ::GetLastError();
}
#endif//_WIN32

/**
 * relFileName
 *
 * finds the path segment /src/ by reverse search on input
 * then returns everything after /src/
 *  C:\Users\Michael\daw\src\host\vst_host.cpp -> \host\vst_host.cpp
 */
const char* relFileName(const char* input) {
    if (input) {
        size_t inLen    = strlen(input);
        const char* pos = input + inLen;
        while (pos >= input) {
            if (*pos == '\\' || *pos == '/') {
                const char* pos2 = pos - 1;
                while (pos2 >= input) {
                    if (*pos2 == '\\' || *pos2 == '/') {
                        if (!strncmp(pos2 + 1, "src", 3))
                            return math::min(input + inLen - 1, pos + 1);
                        break;
                    }
                    pos2--;
                }
            }
            pos--;
        }
    }
    return input;
}

void replaceBackslashInString(String& str) {
    size_t inLen = str.length();
    size_t i     = 0;
    for (; i < inLen; i++) {
        auto& charAt = str.at(i);
        if (charAt == '\\')
            charAt = '/';
    }
}

void replaceBackslashWithForwardslash(const char* filename, char* buf, size_t bufOutSize) {
    size_t inLen   = strlen(filename);
    char* out      = &buf[0];
    const char* in = &filename[0];

    size_t i = 0;
    for (; i < inLen && i < bufOutSize; i++) {
        *out++ = (*in == '\\') ? '/' : *in;
        in++;
    }
    if (i && i == bufOutSize) {
        i--;
    }
    buf[i] = '\0';
}
const char* removeLeadingPathSegments(const char* input, int maxPathSegs) {
    if (input) {
        size_t inLen    = strlen(input);
        const char* pos = input + inLen;
        while (pos >= input) {
            if (*pos == '\\' || *pos == '/') {
                if (--maxPathSegs <= 0) {
                    return pos + 1;
                }
            }
            pos--;
        }
    }
    return input;
}

/**
 * templated version of str_char_toupper_comparator so it works with both char and wchar_t
 */
template<typename charT>
struct str_char_toupper_comparator {
    str_char_toupper_comparator(const std::locale& loc) : loc_(loc) {}
    bool operator()(charT ch1, charT ch2) {
        return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
    }

private:
    const std::locale& loc_;
};

/**
 * find substring (case insensitive)
 */
int StringContainsCI(const String& str1, const String& str2, const std::locale& loc) {

    auto it = std::search( str1.begin(), str1.end(), str2.begin(), str2.end(),
                          str_char_toupper_comparator<typename String::value_type>(loc) );
    if (it != str1.end())
        return (int)(it - str1.begin());
    else
        return -1;// not found
}
