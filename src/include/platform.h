#pragma once
#include <stdint.h>
#include <vector>
#include "str_util.h"

uint64_t getTimeMillis();
double getTimeHPC();
double getSince(double& d);
int64_t getTimeHPint64();

void setMinimumResolutionTimer();

void allocConsole();
void setExceptionHandler();

String getKeyName(int scancode);

void threadSleep(int millis);
