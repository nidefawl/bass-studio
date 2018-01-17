#ifdef __linux__
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
#include "seq_math.h"
#include "str_util.h"

using std::max;
using std::min;//make code analyzer happy (and make author sad)

uint64_t getTimeMillis() {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	return tp.tv_sec * 1000L + tp.tv_usec / 1000L;
}

void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

double getTimeHPC()
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1000000000.0;
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

String getKeyName(int scancode) {
	//TODO: implement linux
	return StringFormat("key_%d", scancode);
}
void threadSleep(int millis) {

	std::this_thread::sleep_for(std::chrono::milliseconds(200));
}
#endif
