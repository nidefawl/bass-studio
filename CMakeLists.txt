cmake_minimum_required(VERSION 3.3)
message (STATUS "CMake version: ${CMAKE_VERSION}")

find_path(DAW_DEPS_DIR PATHS ${DAW_DEPS_DIR})
if(NOT DAW_DEPS_DIR)
  message(FATAL_ERROR "Can't find DAW_DEPS_DIR")
endif()
message (STATUS "DAW_DEPS_DIR: ${DAW_DEPS_DIR}")

project(DAW)
file(GLOB_RECURSE CPP_ALL_FILES src/*.cpp)
file(GLOB_RECURSE C_SRC_FILES src/*.c)
list(APPEND CPP_SRC_FILES "")
foreach(f ${CPP_ALL_FILES})
    if (f MATCHES "/scanner.cpp$")
        continue()
    endif()
    if (f MATCHES "/test/[^/]*$")
        continue()
    endif()
    list(APPEND CPP_SRC_FILES ${f})
endforeach()
message (STATUS "CPP_SRC_FILES = ${CPP_SRC_FILES}")

#list(FILTER CPP_SRC_FILES EXCLUDE REGEX "/scanner.cpp$")
#list(FILTER CPP_SRC_FILES EXCLUDE REGEX "/test/[^/]*$")


SET(DAW_COMPILE_FLAGS -DPA_USE_ASIO=1 -DPA_USE_DS=1 -DVST_FORCE_DEPRECATED=0)

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
message (STATUS "GLFW_LIB = ${GLFW_LIB}")
message (STATUS "SQLITECPP_LIB = ${SQLITECPP_LIB}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-g -O0 -fstack-protector -Wall -Wno-inconsistent-missing-override)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-g -O0 -fstack-protector -Wall -Wno-sign-compare)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    if (SQLITECPP_BUILD_TESTS)
        message(STATUS "Linking against multithread static C++ runtime for unit tests with googletest")
        set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} /MT")
        set(CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG} /MTd")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif (SQLITECPP_BUILD_TESTS)
endif()
add_executable(DAW ${CPP_SRC_FILES} ${C_SRC_FILES})
add_definitions(${DAW_COMPILE_FLAGS})

include_directories(SYSTEM "src/include")
include_directories(SYSTEM "src/nanovg")
include_directories(
    ${DAW_DEPS_DIR}/build/lib/glfw/include
    ${DAW_DEPS_DIR}/build/lib/SQLiteCpp/include
    ${DAW_DEPS_DIR}/glad/include
    ${DAW_DEPS_DIR}/glad/src
    ${DAW_DEPS_DIR}/glm
    ${DAW_DEPS_DIR}/PortAudio/include
    ${DAW_DEPS_DIR}/PortAudio/src/common
    ${DAW_DEPS_DIR}/cereal
    ${DAW_DEPS_DIR}/nvwa/nvwa)
if (WIN32)
    include_directories(${DAW_DEPS_DIR}/ASIOSDK2.3/common
        ${DAW_DEPS_DIR}/ASIOSDK2.3/host
        ${DAW_DEPS_DIR}/ASIOSDK2.3/host/pc
        ${DAW_DEPS_DIR}/PortAudio/src/os/win
        ${DAW_DEPS_DIR}/PortAudio/src/hostapi/asio
        ${DAW_DEPS_DIR}/PortAudio/src/hostapi/dsound)
endif()
target_link_libraries(DAW PRIVATE ${GLFW_LIB} ${SQLITECPP_LIB} ${SQLITE3_LIB})
target_link_libraries(DAW PRIVATE winmm kernel32 gdi32 ole32 uuid comdlg32 ssp)
