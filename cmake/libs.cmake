
# check if build.py is present, not actually used 
find_path(DAW_DEPS_PATH NAMES "build.py" PATHS "${CMAKE_SOURCE_DIR}/../daw-deps" "${CMAKE_BINARY_DIR}/../daw-deps" REQUIRED)
if(NOT DAW_DEPS_PATH)
  message(FATAL_ERROR "Can't find DAW_DEPS_PATH")
endif()
message(STATUS "DAW_DEPS_PATH ${DAW_DEPS_PATH}")

if (USE_SHARED_LIBS)
  set(BUILD_PATH_LIB_TYPE "shared")
else()
  set(BUILD_PATH_LIB_TYPE "static")
endif()

string(TOLOWER "lib-${CMAKE_CXX_COMPILER_ID}-debug-${BUILD_PATH_LIB_TYPE}" DAW_DEPS_BUILD_LIBS_DEBUG)
string(TOLOWER "lib-${CMAKE_CXX_COMPILER_ID}-release-${BUILD_PATH_LIB_TYPE}" DAW_DEPS_BUILD_LIBS_RELEASE)

find_path(DAW_DEPS_INSTALL 
  NAMES
    "${DAW_DEPS_BUILD_LIBS_DEBUG}" 
    "${DAW_DEPS_BUILD_LIBS_RELEASE}" 
  PATHS 
    "${DEPS_BUILD_FOLDER}"
    "${CMAKE_SOURCE_DIR}/../build-deps/install"
    "${CMAKE_BINARY_DIR}/../build-deps/install"
)

if(NOT DAW_DEPS_INSTALL)
  message(FATAL_ERROR "Can't find DAW_DEPS_INSTALL")
endif()
message(STATUS "DAW_DEPS_INSTALL ${DAW_DEPS_INSTALL}")

set(BUILD_PATH_LIB_DEBUG "${DAW_DEPS_INSTALL}/${DAW_DEPS_BUILD_LIBS_DEBUG}")
set(BUILD_PATH_LIB_RELEASE "${DAW_DEPS_INSTALL}/${DAW_DEPS_BUILD_LIBS_RELEASE}")

message(STATUS "BUILD_PATH_LIB_DEBUG ${BUILD_PATH_LIB_DEBUG}")
message(STATUS "BUILD_PATH_LIB_RELEASE ${BUILD_PATH_LIB_RELEASE}")

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
    SOXR_LIB_DEBUG
    NAMES "soxrd.dll" "soxrd"
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

# Need both versions to make CMake happy
if(NOT GLFW_LIB_DEBUG)
    message(FATAL_ERROR "glfw3 not found in ${GLFW_LIB_DEBUG}/glfw/lib")
endif()
if(NOT GLFW_LIB_RELEASE)
    message(FATAL_ERROR "glfw3 not found in ${BUILD_PATH_LIB_RELEASE}/glfw/lib")
endif()

# message(STATUS "Looking in ${BUILD_PATH_LIB_DEBUG}")
set(PYBIND11_CPP_STANDARD -std=c++14)
find_package(pybind11 REQUIRED PATHS "${BUILD_PATH_LIB_DEBUG}/pybind11" "${BUILD_PATH_LIB_RELEASE}/pybind11") 

# message (STATUS "glfw3 debug = ${GLFW_LIB_DEBUG}")
# message (STATUS "glfw3 release = ${GLFW_LIB_RELEASE}")
# message (STATUS "SQLiteCpp debug = ${SQLITECPP_LIB_DEBUG}")
# message (STATUS "SQLiteCpp release = ${SQLITECPP_LIB_RELEASE}")
# message (STATUS "soxr debug = ${SOXR_LIB_DEBUG}")
# message (STATUS "soxr release = ${SOXR_LIB_RELEASE}")
# message (STATUS "portaudio debug = ${PORTAUDIO_LIB_DEBUG}")
# message (STATUS "portaudio release = ${PORTAUDIO_LIB_RELEASE}")
# message (STATUS "portmidi debug = ${PORTMIDI_LIB_DEBUG}")
# message (STATUS "portmidi release = ${PORTMIDI_LIB_RELEASE}")

FIND_PACKAGE ( Threads REQUIRED )
if (LINUX)
  find_package(X11 REQUIRED)
  find_package(ALSA REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
endif(LINUX)
if (APPLE)
FIND_LIBRARY(APPLE_FOUNDATION_LIBRARY Foundation REQUIRED)
FIND_LIBRARY(APPLE_FOUNDATION_LIBRARY CoreFoundation REQUIRED)
  FIND_LIBRARY(APPLE_COREAUDIO_LIBRARY CoreAudio REQUIRED)
  FIND_LIBRARY(APPLE_AUDIOTOOLBOX_LIBRARY AudioToolbox REQUIRED)
  FIND_LIBRARY(APPLE_AUDIOUNIT_LIBRARY AudioUnit REQUIRED)
  FIND_LIBRARY(APPLE_CARBON_LIBRARY Carbon REQUIRED)
  FIND_LIBRARY(APPLE_COCOA_LIBRARY Cocoa REQUIRED)
  FIND_LIBRARY(APPLE_IOKIT_LIBRARY IOKit REQUIRED)
  FIND_LIBRARY(APPLE_OPENGL_LIBRARY OpenGL REQUIRED)
  FIND_LIBRARY(APPLE_COREVIDEO_LIBRARY CoreVideo REQUIRED)
  FIND_LIBRARY(APPLE_COREMIDI_LIBRARY CoreMidi REQUIRED)

endif(APPLE)

if (LINUX)
  link_directories(${GTK3_LIBRARY_DIRS})
  add_definitions(${GTK3_CFLAGS_OTHER})
  include_directories(SYSTEM ${GTK3_INCLUDE_DIRS})
  include_directories(SYSTEM ${X11_X11_INCLUDE_PATH})
  include_directories(SYSTEM ${ALSA_INCLUDE_DIR})
endif(LINUX)


set(USE_SHARED_LIBS Off)
if (USE_SHARED_LIBS)
    set(LIB_LINKAGE "shared")
else()
    set(LIB_LINKAGE "static")
endif(USE_SHARED_LIBS)

string(TOLOWER "lib-${CMAKE_CXX_COMPILER_ID}" LIB_COMPILER)

set(LIB_GN_EXPR ${LIB_COMPILER}-$<$<CONFIG:Debug>:debug>$<$<NOT:$<CONFIG:Debug>>:release>-${LIB_LINKAGE})

include_directories("${DAW_SRC_PATH}")
include_directories("${DAW_SRC_PATH}/include")
include_directories(SYSTEM "${DAW_SRC_PATH}/nanovg")
include_directories(SYSTEM  
    ${DAW_DEPS_INSTALL}/${LIB_GN_EXPR}/glfw/include
    ${DAW_DEPS_INSTALL}/${LIB_GN_EXPR}/SQLiteCpp/include
    ${DAW_DEPS_INSTALL}/${LIB_GN_EXPR}/soxr/include
    ${DAW_DEPS_PATH}/glad/include
    ${DAW_DEPS_PATH}/glad/src
    ${DAW_DEPS_PATH}/glm
    ${DAW_DEPS_PATH}/portaudio/include
    ${DAW_DEPS_PATH}/portmidi/pm_common
    ${DAW_DEPS_PATH}/SplineLibrary/spline_library
    ${DAW_DEPS_PATH}/cereal
    ${DAW_DEPS_PATH}/kissfft)