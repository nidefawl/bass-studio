
set(DEPS_BUILD_FOLDER "${DAW_DEPS_PATH}/build" CACHE STRING "build subdirectory")

find_library(
    GLFW_LIB 
    NAMES "glfw3" "glfw3dll"
    PATHS ${DEPS_BUILD_FOLDER}/lib/glfw/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    SQLITECPP_LIB NAMES "SQLiteCpp"
    PATHS ${DEPS_BUILD_FOLDER}/lib/SQLiteCpp/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    SOXR_LIB NAMES "soxr"
    PATHS ${DEPS_BUILD_FOLDER}/lib/soxr/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    PORTAUDIO_LIB NAMES "portaudio" "portaudio.dll" "portaudio_static_x64"
    PATHS ${DEPS_BUILD_FOLDER}/lib/portaudio/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
message (STATUS "glfw3 = ${GLFW_LIB}")
message (STATUS "SQLiteCpp = ${SQLITECPP_LIB}")
message (STATUS "soxr = ${SOXR_LIB}")
message (STATUS "portaudio = ${PORTAUDIO_LIB}")

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