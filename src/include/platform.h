#pragma once
#include "types.h"
#include <vector>
#include "str_util.h"
#if defined(__linux__) || defined(__APPLE__)
#include "platform/linux/windowsize.h"
#endif
#ifdef _WIN32
#include "platform/win/windowsize.h"
#endif

#if defined(_WIN32)
#define FILE_PATHSEP_CHAR '\\'
#define FILE_PATHSEP_STR "\\"
#define FILE_PATHSEP_STR_WIDE L"\\"
#else
#define FILE_PATHSEP_CHAR '/'
#define FILE_PATHSEP_STR "/"
#define FILE_PATHSEP_WIDE L"/"
#endif
#if defined(_WIN32)
#define DAW_PLATFORM_VST2_PATH_DEFAULT "C:\\Program Files\\Steinberg\\VstPlugins"
#define DAW_PLATFORM_CLAP_PATH_DEFAULT "C:\\Program Files\\Common Files\\CLAP"
#define DAW_PLATFORM_VST3_PATH_DEFAULT "C:\\Program Files\\Common Files\\VST3"
#elif defined(__APPLE__)
#define DAW_PLATFORM_VST2_PATH_DEFAULT "/Library/Audio/Plug-Ins/VST"
#define DAW_PLATFORM_CLAP_PATH_DEFAULT "/Library/Audio/Plug-Ins/CLAP"
#define DAW_PLATFORM_VST3_PATH_DEFAULT "/Library/Audio/Plug-Ins/VST3"
#else
#define DAW_PLATFORM_VST2_PATH_DEFAULT "~/.vst"
#define DAW_PLATFORM_CLAP_PATH_DEFAULT "~/.clap"
#define DAW_PLATFORM_VST3_PATH_DEFAULT "~/.vst3"
#endif

#define FILE_PATHSEP_FORWARD_CHAR '/'
#define FILE_PATHSEP_FORWARD_STR "/"

double getTimeSecondsD();
int64_t getTimeMillis();
double getTimeMillisD();
float getTimeMillisF();
int64_t getTimeMicros();


void setMinimumResolutionTimer();

void showProgramConsole();
void enableVirtTermProc();
void setExceptionHandler();
String getKeyName(int scancode);

void logStackTrace();
void getStackTrace(std::vector<String>& vec);

namespace App::Platform {
bool determineUserdataPath(String& path);
String GetExecutablePath();
String getCurrentWorkingDirectory();

String GetResourcePath();
String GetUserdataPath();
String GetDefaultSettingFilesPath();

String toResourcePath(const String& relPath);
String toUserdataPath(const String& relPath);
String toDefaultSettingFilesPath(const String& relPath);

void setResourcePath(String cwd);
void setUserdataPath(String pathHomeOrAppData);
void setDefaultSettingFilesPath(String cwd);

void initPlatformEnvironment(const String& appname, const String& optionalCwd = "");
/**
 * @brief Appends final path seperator
 *        Win32 only: Replace forward slash with backslash
 * @param pathString path to a directory
 */
void sanitizePathToDirectory(String& pathString);
void sanitizePathToDirectoryWide(WString& pathString);
void shellExpandPath(String& pathString);

/**
 * @brief Win32 only: Replace forward slash with backslash
 * @param pathString path to a file
 */
void sanitizePathToFile(String& pathString);
void sanitizePathToFileWide(WString& pathString);
int32_t createUniqueFilename(String& pathString, const String& baseName);
}
