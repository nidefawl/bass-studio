
if (USE_SHARED_LIBS)
set(BUILD_PATH_LIB_TYPE "shared")
else()
set(BUILD_PATH_LIB_TYPE "static")
endif()
set(BUILD_PATH_LIB_RELEASE "lib-${CMAKE_CXX_COMPILER_ID}-release-${BUILD_PATH_LIB_TYPE}")
set(BUILD_PATH_LIB_DEBUG "lib-${CMAKE_CXX_COMPILER_ID}-debug-${BUILD_PATH_LIB_TYPE}")
string(TOLOWER ${BUILD_PATH_LIB_RELEASE} BUILD_PATH_LIB_RELEASE)
string(TOLOWER ${BUILD_PATH_LIB_DEBUG} BUILD_PATH_LIB_DEBUG)
set(BUILD_PATH_LIB_RELEASE "${DEPS_BUILD_FOLDER}/${BUILD_PATH_LIB_RELEASE}")
set(BUILD_PATH_LIB_DEBUG "${DEPS_BUILD_FOLDER}/${BUILD_PATH_LIB_DEBUG}")
message(STATUS "BUILD_PATH_LIB_RELEASE ${BUILD_PATH_LIB_RELEASE}")
message(STATUS "BUILD_PATH_LIB_DEBUG ${BUILD_PATH_LIB_DEBUG}")
find_library(
    GLFW_LIB_RELEASE
    NAMES "glfw3" "glfw3dll"
    PATHS ${BUILD_PATH_LIB_RELEASE}/glfw/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    SQLITECPP_LIB_RELEASE NAMES "SQLiteCpp"
    PATHS ${BUILD_PATH_LIB_RELEASE}/SQLiteCpp/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    SOXR_LIB_RELEASE NAMES "soxr"
    PATHS ${BUILD_PATH_LIB_RELEASE}/soxr/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    PORTAUDIO_LIB_RELEASE NAMES "portaudio" "portaudio.dll" "portaudio_static_x64"
    PATHS ${BUILD_PATH_LIB_RELEASE}/portaudio/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    PORTMIDI_LIB_RELEASE NAMES "portmidi_s"
    PATHS ${BUILD_PATH_LIB_RELEASE}/portmidi/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    GLFW_LIB_DEBUG
    NAMES "glfw3" "glfw3dll"
    PATHS ${BUILD_PATH_LIB_DEBUG}/glfw/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    SQLITECPP_LIB_DEBUG NAMES "SQLiteCpp"
    PATHS ${BUILD_PATH_LIB_DEBUG}/SQLiteCpp/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    SOXR_LIB_DEBUG NAMES "soxr"
    PATHS ${BUILD_PATH_LIB_DEBUG}/soxr/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    PORTAUDIO_LIB_DEBUG NAMES "portaudio" "portaudio.dll" "portaudio_static_x64"
    PATHS ${BUILD_PATH_LIB_DEBUG}/portaudio/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    PORTMIDI_LIB_DEBUG NAMES "portmidi_s"
    PATHS ${BUILD_PATH_LIB_DEBUG}/portmidi/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)

message (STATUS "glfw3 debug = ${GLFW_LIB_DEBUG}")
message (STATUS "glfw3 release = ${GLFW_LIB_RELEASE}")
message (STATUS "SQLiteCpp debug = ${SQLITECPP_LIB_DEBUG}")
message (STATUS "SQLiteCpp release = ${SQLITECPP_LIB_RELEASE}")
message (STATUS "soxr debug = ${SOXR_LIB_DEBUG}")
message (STATUS "soxr release = ${SOXR_LIB_RELEASE}")
message (STATUS "portaudio debug = ${PORTAUDIO_LIB_DEBUG}")
message (STATUS "portaudio release = ${PORTAUDIO_LIB_RELEASE}")
message (STATUS "portmidi debug = ${PORTMIDI_LIB_DEBUG}")
message (STATUS "portmidi release = ${PORTMIDI_LIB_RELEASE}")

FIND_PACKAGE ( Threads REQUIRED )
if (UNIX)
  find_package(X11 REQUIRED)
  find_package(ALSA REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
endif(UNIX)

if (UNIX)
  link_directories(${GTK3_LIBRARY_DIRS})
  add_definitions(${GTK3_CFLAGS_OTHER})
  include_directories(SYSTEM ${GTK3_INCLUDE_DIRS})
  include_directories(SYSTEM ${X11_X11_INCLUDE_PATH})
  include_directories(SYSTEM ${ALSA_INCLUDE_DIR})
endif(UNIX)