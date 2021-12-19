#if defined(__linux__) || defined (__APPLE__)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <vector>
#include <time.h>
#include <thread>

#include "msgbox.h"
#include "str_util.h"
#include "threads.h"

uint64_t getTimeMillis() {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000L + tp.tv_usec / 1000L;
}
double getTimeMillisd() {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000.0 + tp.tv_usec / 1000.0;
}


double getTimeHPC()
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1000000000.0;
}
int64_t getTimeHPint64()
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	int64_t microsecs = t.tv_sec*1000000;
	int64_t microsecs2 = t.tv_nsec/1000;
	return microsecs+microsecs2;
}
double getSince(double& d) //checks for overflow
{
	double now = getTimeHPC();
	if (now < d) d = now;
	return now - d;
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

String FormatErrorMessage(int32_t error, String msg)
{
	static const int BUFFERLENGTH = 1024;
	std::vector<char> buf(BUFFERLENGTH);
	char* strErrBuf = strerror_r(error, buf.data(), BUFFERLENGTH);
	if (strErrBuf)
	{
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

bool determineUserdataPath(String& path)
{

	char* homedir = getenv("HOME");
	if (!homedir) {
		uid_t curUid = getuid();
		struct passwd * curPw = getpwuid(curUid);
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
