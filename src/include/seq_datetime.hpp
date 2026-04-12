#include "str_util.hpp"

// Returns current UTC time as Unix timestamp (seconds since epoch)
// Historical note: Unix time_t has been int32 on 32-bit systems — explicitly
// casting to int64_t future-proofs against the 2038 overflow problem
inline int64_t GetInt64GMTDate()
{
    return static_cast<int64_t>(std::time(nullptr));
}

// Converts a Unix timestamp to a localized date-time string
// Uses localtime_r (POSIX) on Linux/macOS, localtime_s (Windows) on MSVC
// Both are thread-safe reentrant versions — unlike the classic localtime()
inline String GetInt64DateAsLocalizedTimeStr(int64_t timestamp)
{
    std::time_t t = static_cast<std::time_t>(timestamp);
    std::tm tm_buf{};

#if defined(_MSC_VER) || defined(__MINGW32__)
    localtime_s(&tm_buf, &t);   // Windows: args are reversed vs POSIX!
#else
    localtime_r(&t, &tm_buf);   // POSIX: Linux, macOS, etc.
#endif

    char buf[32];
    std::snprintf(buf, sizeof(buf),
        "%04d-%02d-%02d %02d:%02d",
        tm_buf.tm_year + 1900,
        tm_buf.tm_mon  + 1,
        tm_buf.tm_mday,
        tm_buf.tm_hour,
        tm_buf.tm_min);

    return String(buf);  // replace with your string type
}
