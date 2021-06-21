#pragma once
#include <stdint.h>
#include <vector>
#include "str_util.h"

uint64_t getTimeMillis();
double getTimeMillisd();
double getTimeHPC();
double getSince(double& d);
int64_t getTimeHPint64();

void setMinimumResolutionTimer();

void allocConsole();
void setExceptionHandler();
bool determineUserdataPath(String& path);
String getKeyName(int scancode);

void threadSleep(int millis);
void logStackTrace();
void getStackTrace(std::vector<String>& vec);


String toResourcePath(String relPath);
void setResourcePath(String cwd);
void setUserdataPath(String cwd);
String toUserdataPath(String relPath);