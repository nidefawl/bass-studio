#if defined(__linux__) || defined(__APPLE__)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cstdio>
#include <vector>
#include <ctime>
#include <thread>

#include "msgbox.h"
#include "str_util.h"
#include "threads.h"


void timespec_diff(struct timespec* start, struct timespec* stop, struct timespec* result); 

namespace {
struct timespec getTimeCurrent() {
    struct timespec tmSpecCurrent{};
    clock_gettime(CLOCK_MONOTONIC, &tmSpecCurrent);
    return tmSpecCurrent;
}

struct timespec& getTimeBegin() {
    static struct timespec tmSpecStart = getTimeCurrent();
    return tmSpecStart;
}

struct timespec getTimeSinceBegin() {
    auto tmBegin = getTimeBegin();
    auto tmTemp = getTimeCurrent();
    timespec_diff(&tmBegin, &tmTemp, &tmTemp);
    return tmTemp;
}
}

double getTimeSecondsD() {
    auto tmTime = getTimeSinceBegin();
    return tmTime.tv_sec + tmTime.tv_nsec / 1.0e9;
}

int64_t getTimeMillis() {
    auto tmTime = getTimeSinceBegin();
    return tmTime.tv_sec * 1'000L + tmTime.tv_nsec / 1'000'000L;
}

double getTimeMillisD() {
    auto tmTime = getTimeSinceBegin();
    double tmDoubleMillis = static_cast<double>(tmTime.tv_sec) * 1000.0;
    auto nSecsToMillis = tmTime.tv_nsec / 1'000'000L;
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
    char* strErrBuf = strerror_r(error, buf.data(), BUFFERLENGTH);
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

String getCurrentWorkingDirectory() {
    String path;
    char* cwdBuf = getcwd(NULL, 0);
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

#endif
#endif
