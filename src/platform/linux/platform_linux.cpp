#if defined(__linux__) || defined (__APPLE__)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <vector>
#include <sys/time.h>
#include <time.h>
#include <thread>
#include <chrono>

#include "msgbox.h"
#include "str_util.h"

uint64_t getTimeMillis() {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000L + tp.tv_usec / 1000L;
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
void threadSleep(int millis) {

	std::this_thread::sleep_for(std::chrono::milliseconds(200));
}


String FormatErrorMessage(int32_t error, String msg)
{
	static const int BUFFERLENGTH = 1024;
	std::vector<char> buf(BUFFERLENGTH);
	strerror_r(error, buf.data(), BUFFERLENGTH);
	if (msg.empty())
		return String(buf.data());
	return String(buf.data()) + "   (" + msg + ")";
}
#endif
