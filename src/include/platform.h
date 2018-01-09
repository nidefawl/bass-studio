#pragma once
#include <stdint.h>
#include <vector>
#include "str_util.h"

uint64_t getTimeMillis();
double getTimeHPC();
double getSince(double& d);

void setMinimumResolutionTimer();
String getLastWin32ErrorString();

void allocConsole();
void setExceptionHandler();

String getKeyName(int scancode);
