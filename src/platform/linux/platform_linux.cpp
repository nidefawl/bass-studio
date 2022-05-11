#if defined(__linux__) || defined(__APPLE__)
#include "types.h"
#include <sys/time.h>
#include <sys/types.h>
#include "msgbox.h"
#include "str_util.h"
#include "platform.h"
#ifdef __linux__
#include <unistd.h>
#include <pwd.h>
#endif
#include <wordexp.h>

void timespec_diff(struct timespec* start, struct timespec* stop, struct timespec* result) {
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec  = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1'000'000'000L;
    } else {
        result->tv_sec  = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
}

namespace {
    struct timespec getTimeCurrent() {
        struct timespec tmSpecCurrent {};
        clock_gettime(CLOCK_MONOTONIC, &tmSpecCurrent);
        return tmSpecCurrent;
    }

    struct timespec& getTimeBegin() {
        static struct timespec tmSpecStart = getTimeCurrent();
        return tmSpecStart;
    }

    struct timespec getTimeSinceBegin() {
        auto tmBegin = getTimeBegin();
        auto tmTemp  = getTimeCurrent();
        timespec_diff(&tmBegin, &tmTemp, &tmTemp);
        return tmTemp;
    }
}// namespace

double getTimeSecondsD() {
    auto tmTime = getTimeSinceBegin();
    return tmTime.tv_sec + tmTime.tv_nsec / 1.0e9;
}

int64_t getTimeMillis() {
    auto tmTime = getTimeSinceBegin();
    return tmTime.tv_sec * 1'000L + tmTime.tv_nsec / 1'000'000L;
}

double getTimeMillisD() {
    auto tmTime           = getTimeSinceBegin();
    double tmDoubleMillis = static_cast<double>(tmTime.tv_sec) * 1000.0;
    auto nSecsToMillis    = tmTime.tv_nsec / 1'000'000L;
    return tmDoubleMillis + static_cast<double>(nSecsToMillis);
}
float getTimeMillisF() {
    return static_cast<float>(getTimeMillisD());
}

int64_t getTimeMicros() {
    auto tmTime = getTimeSinceBegin();
    return tmTime.tv_sec * 1'000'000L + tmTime.tv_nsec / 1'000L;
}

void setMinimumResolutionTimer() {
}

void allocConsole() {
}

void setExceptionHandler() {
}

String getKeyName(int scancode) {
    //TODO: implement linux
    return StringFormat("key_%d", scancode);
}

String FormatErrorMessage(uint32_t error, const String& msg) {
    static const int BUFFERLENGTH = 1024;
    std::vector<char> buf(BUFFERLENGTH);
    char* strErrBuf = strerror_r((int)error, buf.data(), BUFFERLENGTH);
    if (strErrBuf) {
        String strErrMsg = String(strErrBuf);
        if (!msg.empty()) {
            strErrMsg += " (" + msg + ")";
        }
        return strErrMsg;
    }
    return msg;
}

#ifdef __linux__

namespace App::Platform {

String getCurrentWorkingDirectory() {
    String path;
    char* cwdBuf = getcwd(nullptr, 0);
    if (cwdBuf) {
        path = cwdBuf;
    }
    free(cwdBuf);
    return path;
}

bool determineUserdataPath(String& path) {
    char* homedir = getenv("HOME");
    if (!homedir) {
        uid_t curUid         = getuid();
        struct passwd* curPw = getpwuid(curUid);
        if (curPw) {
            homedir = curPw->pw_dir;
        }
    }
    if (homedir) {
        path = homedir;
        return true;
    }
    return false;
}

void sanitizePathToDirectory(String& pathString) {
    if (pathString.length() && !StrEndsWith(pathString, FILE_PATHSEP_STR))
        pathString += FILE_PATHSEP_STR;
}

void shellExpandPath(String& pathString) {
    wordexp_t result{};
    switch (wordexp(StringAsCStr(pathString), &result, WRDE_NOCMD)) {
        case 0:
            pathString = result.we_wordv[0];
            [[fallthrough]];
        case WRDE_NOSPACE:
            /* If the error was WRDE_NOSPACE,
         then perhaps part of the result was allocated.  */
            wordfree(&result);
        default: /* Some other error.  */
            break;
    }
}

void sanitizePathToFile(String& pathString) {
}

} // namespace App::Platform

#endif // __linux__

#endif // defined(__linux__) || defined(__APPLE__)
