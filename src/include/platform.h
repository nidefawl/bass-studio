#pragma once
#include <cstdint>
#include <vector>
#include "str_util.h"

double getTimeSecondsD();
int64_t getTimeMillis();
double getTimeMillisD();
float getTimeMillisF();
int64_t getTimeMicros();


void setMinimumResolutionTimer();

void allocConsole();
void setExceptionHandler();
bool determineUserdataPath(String& path);
String getKeyName(int scancode);

void logStackTrace();
void getStackTrace(std::vector<String>& vec);


String toResourcePath(const String& relPath);
void setResourcePath(String cwd);
void setUserdataPath(String cwd);
String toUserdataPath(const String& relPath);
String getCurrentWorkingDirectory();
