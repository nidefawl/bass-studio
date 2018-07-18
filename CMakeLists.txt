cmake_minimum_required(VERSION 3.3)
message (STATUS "CMake version: ${CMAKE_VERSION}")

find_path(DAW_DEPS_DIR PATHS ${DAW_DEPS_DIR})
if(NOT DAW_DEPS_DIR)
  message(FATAL_ERROR "Can't find DAW_DEPS_DIR")
endif()
message (STATUS "DAW_DEPS_DIR: ${DAW_DEPS_DIR}")

option(BUILD_HOST "Build daw" ON)
option(BUILD_SCANNER "Build plugin scanner" OFF)

project(DAW C CXX)
file(GLOB_RECURSE CPP_ALL_FILES src/*.cpp)
file(GLOB_RECURSE C_SRC_FILES src/*.c)
list(APPEND CPP_SRC_FILES "")
list(APPEND SCANNER_SRC_FILES "")
foreach(f ${CPP_ALL_FILES})
    if (f MATCHES ".*/scanner.cpp$")
        list(APPEND SCANNER_SRC_FILES ${f})
        continue()
    endif()
    if (f MATCHES ".*/host/main.cpp$")
        list(APPEND CPP_SRC_FILES ${f})
        continue()
    endif()
    if (f MATCHES ".*/test/[^/]*$")
        continue()
    endif()
    if (UNIX)
	    if (f MATCHES ".*/host/platform/win/[^/]*$")
	        continue()
	    endif()
	    if (f MATCHES ".*/platform/win/[^/]*$")
	        continue()
	    endif()
	    if (f MATCHES ".*/platform/mingw/[^/]*$")
	        continue()
	    endif()
	    if (f MATCHES ".*/external_asio.cpp$")
	        continue()
	    endif()
	  endif (UNIX)
    if (WIN32)
	    if (f MATCHES ".*/host/platform/linux/[^/]*$")
	        continue()
	    endif()
	    if (f MATCHES ".*/platform/linux/[^/]*$")
	        continue()
	    endif()
	  endif (WIN32)
    if (MSVC)
	    if (f MATCHES ".*/platform/mingw/[^/]*$")
	        continue()
	    endif()
	  endif (MSVC)
    list(APPEND CPP_SRC_FILES ${f})
    list(APPEND SCANNER_SRC_FILES ${f})
endforeach()

#list(FILTER CPP_SRC_FILES EXCLUDE REGEX "/scanner.cpp$")
#list(FILTER CPP_SRC_FILES EXCLUDE REGEX "/test/[^/]*$")

  SET(DAW_COMPILE_FLAGS -DVST_FORCE_DEPRECATED=0 -DUSE_GLAD_GL_HEADERS -DGLM_FORCE_CXX14=1 -DNO_LEAK_DETECT)
if (WIN32)
  SET(DAW_COMPILE_FLAGS ${DAW_COMPILE_FLAGS} -DPA_USE_ASIO=1 -DPA_USE_DS=1)
endif(WIN32)
if (UNIX)
  SET(DAW_COMPILE_FLAGS ${DAW_COMPILE_FLAGS} -DPA_USE_ALSA=1)
	set(DAW_COMPILE_FLAGS ${DAW_COMPILE_FLAGS} -D__cdecl="")
endif(UNIX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


find_library(
    GLFW_LIB NAMES "glfw3"
    PATHS ${DAW_DEPS_DIR}/build/lib/glfw/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    SQLITECPP_LIB NAMES "SQLiteCpp"
    PATHS ${DAW_DEPS_DIR}/build/lib/SQLiteCpp/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    SQLITE3_LIB NAMES "sqlite3"
    PATHS ${DAW_DEPS_DIR}/build/lib/SQLiteCpp/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
find_library(
    SOXR_LIB NAMES "soxr"
    PATHS ${DAW_DEPS_DIR}/build/lib/soxr/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)
message (STATUS "glfw3 = ${GLFW_LIB}")
message (STATUS "SQLiteCpp = ${SQLITECPP_LIB}")
message (STATUS "soxr = ${SOXR_LIB}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-g -O0 -fstack-protector -Wall -Wno-inconsistent-missing-override)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-g -O0 -fstack-protector -Wall -Wno-sign-compare)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX /wd4067 /wd4267 /wd4244)
endif()

add_definitions(${DAW_COMPILE_FLAGS})

include_directories("src/include")
include_directories(SYSTEM "src/nanovg")
include_directories(SYSTEM
    ${DAW_DEPS_DIR}/build/lib/glfw/include
    ${DAW_DEPS_DIR}/build/lib/SQLiteCpp/include
    ${DAW_DEPS_DIR}/build/lib/soxr/include
    ${DAW_DEPS_DIR}/glad/include
    ${DAW_DEPS_DIR}/glad/src
    ${DAW_DEPS_DIR}/glm
    ${DAW_DEPS_DIR}/portaudio/include
    ${DAW_DEPS_DIR}/portaudio/src/common
    ${DAW_DEPS_DIR}/cereal
    ${DAW_DEPS_DIR}/nvwa/nvwa)


FIND_PACKAGE ( Threads REQUIRED )
if (WIN32)
    include_directories(${DAW_DEPS_DIR}/ASIOSDK2.3/common
        ${DAW_DEPS_DIR}/ASIOSDK2.3/host
        ${DAW_DEPS_DIR}/ASIOSDK2.3/host/pc
        ${DAW_DEPS_DIR}/portaudio/src/os/win
        ${DAW_DEPS_DIR}/portaudio/src/hostapi/asio
        ${DAW_DEPS_DIR}/portaudio/src/hostapi/dsound)
endif()
if (BUILD_HOST)
add_executable(DAW ${CPP_SRC_FILES} ${C_SRC_FILES})
target_link_libraries(DAW PRIVATE ${GLFW_LIB} ${SQLITECPP_LIB} ${SQLITE3_LIB} ${SOXR_LIB})
endif()
if (BUILD_SCANNER)
add_executable(plugin_scan ${SCANNER_SRC_FILES} ${C_SRC_FILES})
target_link_libraries(plugin_scan PRIVATE ${GLFW_LIB} ${SQLITECPP_LIB} ${SQLITE3_LIB})
endif()
if (UNIX)
	find_package(X11 REQUIRED)
  find_package(ALSA REQUIRED)
  find_package(PkgConfig)

  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  link_directories(${GTK3_LIBRARY_DIRS})
  add_definitions(${GTK3_CFLAGS_OTHER})
  include_directories(SYSTEM ${GTK3_INCLUDE_DIRS})
  include_directories(SYSTEM ${X11_X11_INCLUDE_PATH})
  include_directories(SYSTEM ${ALSA_INCLUDE_DIR})
  if (BUILD_HOST)
  target_link_libraries(DAW PRIVATE ${X11_X11_LIB} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} ${ALSA_LIBRARY} ${GTK3_LIBRARIES})
  target_link_libraries(DAW PRIVATE rt m)
  endif()
  if (BUILD_SCANNER)
  target_link_libraries(plugin_scan PRIVATE ${X11_X11_LIB} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} ${ALSA_LIBRARY} ${GTK3_LIBRARIES})
  target_link_libraries(plugin_scan PRIVATE rt m)
  endif()
  message(STATUS "GTK3_LIBRARY_DIRS: ${GTK3_LIBRARY_DIRS}")
  message(STATUS "GTK3_LIBRARIES: ${GTK3_LIBRARIES}")
  message(STATUS "GTK3_INCLUDE_DIRS: ${GTK3_INCLUDE_DIRS}")
  message(STATUS "X11_X11_INCLUDE_PATH: ${X11_X11_INCLUDE_PATH}")
  message(STATUS "X11_X11_LIB: ${X11_X11_LIB}")
  message(STATUS "ALSA_INCLUDE_DIR: ${ALSA_INCLUDE_DIR}")
  message(STATUS "ALSA_LIBRARY: ${ALSA_LIBRARY}")
  include_directories(
      ${DAW_DEPS_DIR}/portaudio/src/os/unix
      ${DAW_DEPS_DIR}/portaudio/src/hostapi/alsa)
endif(UNIX)
if (WIN32)
  if (BUILD_HOST)
  target_link_libraries(DAW PRIVATE winmm kernel32 gdi32 ole32 uuid comdlg32)
  endif()
  if (BUILD_SCANNER)
  target_link_libraries(plugin_scan PRIVATE winmm kernel32 gdi32 ole32 uuid comdlg32)
  endif()
endif(WIN32)
