#pragma once
#include "types.h"
#include <vector>
#include "str_util.h"

#ifdef _WIN32
#define FILE_PATHSEP_CHAR '\\'
#define FILE_PATHSEP_STR "\\"
#define DAW_PLATFORM_VST2_PATH_DEFAULT "C:\\Program Files\\Steinberg\\VstPlugins"
#define DAW_PLATFORM_CLAP_PATH_DEFAULT "C:\\Program Files\\Common Files\\CLAP"
#else
#define FILE_PATHSEP_CHAR '/'
#define FILE_PATHSEP_STR "/"
#define DAW_PLATFORM_VST2_PATH_DEFAULT "~/.vst"
#define DAW_PLATFORM_CLAP_PATH_DEFAULT "~/.clap"
#endif

#define FILE_PATHSEP_FORWARD_CHAR '/'
#define FILE_PATHSEP_FORWARD_STR "/"

double getTimeSecondsD();
int64_t getTimeMillis();
double getTimeMillisD();
float getTimeMillisF();
int64_t getTimeMicros();


void setMinimumResolutionTimer();

void allocConsole();
void enableVirtTermProc();
void setExceptionHandler();
String getKeyName(int scancode);

void logStackTrace();
void getStackTrace(std::vector<String>& vec);

namespace App::Platform {
bool determineUserdataPath(String& path);
String GetExecutablePath();
String getCurrentWorkingDirectory();

String toResourcePath(const String& relPath);
String toUserdataPath(const String& relPath);
void setResourcePath(String cwd);
String GetResourcePath();
String GetUserdataPath();
void setUserdataPath(String pathHomeOrAppData);
void initPlatformEnvironment(const String& appname, const String& optionalCwd = "");
/**
 * @brief Appends final path seperator
 *        Win32 only: Replace forward slash with backslash
 * @param pathString path to a directory
 */
void sanitizePathToDirectory(String& pathString);
void shellExpandPath(String& pathString);

/**
 * @brief Win32 only: Replace forward slash with backslash
 * @param pathString path to a file
 */
void sanitizePathToFile(String& pathString);
int32_t createUniqueFilename(String& pathString, const String& baseName);
}
