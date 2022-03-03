#include "str_util.h"
#include <cstdarg>
#include <climits>
#include <vector>
#include "math/seq_math.h"
#include "seq_time.h"
#include "assert_dbg.h"

#ifdef _WIN32
#include <Windows.h>
#endif
#if __linux__
#include <cstdio>
#endif

#if __linux__ || defined(__APPLE__)
int _vscprintf(const char* format, va_list pargs) {
    int retval;
    va_list argcopy;
    va_copy(argcopy, pargs);
    retval = vsnprintf(NULL, 0, format, argcopy);
    va_end(argcopy);
    return retval;
}
#endif

String StringFormat(const char* fmt, ...) {
    char* strp = NULL;
    String str;
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = _________vasprintf(&strp, fmt, ap);
    va_end(ap);
    if (r > 0) {
        str = strp;
    }
    if (strp) {
        free(strp);
    }
    return str;
}
int _________asprintf(char** strp, const char* fmt, ...) {
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = _________vasprintf(strp, fmt, ap);
    va_end(ap);
    return (r);
}

int _________vasprintf(char** strp, const char* fmt, va_list ap) {
    int r = -1, size = _vscprintf(fmt, ap);

    if ((size >= 0) && (size < INT_MAX)) {
        *strp = (char*) malloc(size + 1);//+1 for null
        if (*strp) {
            r = vsnprintf(*strp, size + 1, fmt, ap);//+1 for null
            if ((r < 0) || (r > size)) {
                free(*strp);
                r = -1;
            }
        }
    } else {
        *strp = 0;
    }

    return (r);
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
String tickAsBeatString(int32_t tick) {
    static const size_t buf_size        = 32;
    static thread_local char* const buf = (char*) malloc(buf_size);
    auto beatBarNth                     = tickToBarBeat16th(tick, 4, 2);
    constexpr const char format[]       = "%d.%d.%d.%d";
#ifdef __APPLE__
    snprintf(buf, buf_size, format, beatBarNth.bar + 1, beatBarNth.beat + 1, beatBarNth.th + 1);
#else
    _snprintf_s(buf, buf_size, _TRUNCATE, format, beatBarNth.bar + 1, beatBarNth.beat + 1, beatBarNth.th + 1, beatBarNth.subticks);
#endif
    return String(buf);
}

static const char* const noteNames[12]{
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const char* noteName(int note) {//DONT KEEP REFERENCE
    static const size_t buf_size        = 32;
    static thread_local char* const buf = (char*) malloc(buf_size);
    int noteNameIdx                     = math::clamp(note % 12, 0, 11);
#ifdef __APPLE__
    snprintf(buf, buf_size, "%s%d", noteNames[noteNameIdx], (note / 12) - 2);
#else
    _snprintf_s(buf, buf_size, _TRUNCATE, "%s%d", noteNames[noteNameIdx], (note / 12) - 2);
#endif
    return buf;
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
      WINBASEAPI int WINAPI MultiByteToWideChar (UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);
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
