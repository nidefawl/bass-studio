if(NOT EXISTS ${PROJECT_SRC_PATH})
  message(FATAL_ERROR "Can't find PROJECT_SRC_PATH")
endif()
if (NOT DEFINED MAIN_SRC_PATH)
  set(MAIN_SRC_PATH ${PROJECT_SRC_PATH})
endif()

# check if build.py is present, not actually used 
find_path(PROJECT_DEPS_PATH
    NAMES "build.py"
    PATHS 
        "${CMAKE_SOURCE_DIR}/../daw-deps"
        "${CMAKE_BINARY_DIR}/../daw-deps"
        "D:/dev/daw-deps"
    REQUIRED)

message(STATUS "PROJECT_DEPS_PATH ${PROJECT_DEPS_PATH}")
set(USE_SHARED_LIBS Off)

if (USE_SHARED_LIBS)
    set(LIB_LINKAGE "shared")
else()
    set(LIB_LINKAGE "static")
endif(USE_SHARED_LIBS)

if (USE_SHARED_LIBS)
  set(BUILD_PATH_LIB_TYPE "shared")
else()
  set(BUILD_PATH_LIB_TYPE "static")
endif()

string(TOLOWER "lib-${CMAKE_CXX_COMPILER_ID}-debug-${BUILD_PATH_LIB_TYPE}" DEPS_BUILD_LIBS_DEBUG)
string(TOLOWER "lib-${CMAKE_CXX_COMPILER_ID}-release-${BUILD_PATH_LIB_TYPE}" DEPS_BUILD_LIBS_RELEASE)


string(TOLOWER "lib-${CMAKE_CXX_COMPILER_ID}" LIB_COMPILER)

set(LIB_GN_EXPR ${LIB_COMPILER}-$<IF:$<CONFIG:Debug>,debug,release>-${LIB_LINKAGE})


find_path(PROJECT_DEPS_INSTALL_PATH 
    NAMES
        "${DEPS_BUILD_LIBS_DEBUG}" 
        "${DEPS_BUILD_LIBS_RELEASE}" 
    PATHS 
        "${DEPS_BUILD_FOLDER}"
        "${CMAKE_SOURCE_DIR}/../build-deps/install"
        "${CMAKE_BINARY_DIR}/../build-deps/install"
    REQUIRED
)

set(BUILD_PATH_LIB_DEBUG "${PROJECT_DEPS_INSTALL_PATH}/${DEPS_BUILD_LIBS_DEBUG}")
set(BUILD_PATH_LIB_RELEASE "${PROJECT_DEPS_INSTALL_PATH}/${DEPS_BUILD_LIBS_RELEASE}")

# find_package* and deps cmakes helpers cannot be used until multi config finds widespread support
# until then we use this shitty way of defining DEBUG and RELEASE libs separately and
# taking care of config dependant include paths ourselves

find_library(
    GLFW_LIB_RELEASE
    NAMES "glfw3" "glfw3dll"
    PATHS ${BUILD_PATH_LIB_RELEASE}/glfw/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    KISSFFT_LIB_RELEASE
    NAMES "kissfft-float"
    PATHS ${BUILD_PATH_LIB_RELEASE}/kissfft/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    SQLITECPP_LIB_RELEASE
    NAMES "SQLiteCpp"
    PATHS ${BUILD_PATH_LIB_RELEASE}/SQLiteCpp/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    SQLITE3_LIB_RELEASE
    NAMES "sqlite3"
    PATHS ${BUILD_PATH_LIB_RELEASE}/SQLiteCpp/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
string(TOLOWER "soxr-${CMAKE_CXX_COMPILER_ID}-release" SOXR_RELEASE_DLL_NAME)
find_library(
    SOXR_LIB_RELEASE
    NAMES ${SOXR_RELEASE_DLL_NAME}
    PATHS ${BUILD_PATH_LIB_RELEASE}/soxr/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    PORTAUDIO_LIB_RELEASE
    NAMES "portaudio" "portaudio.dll" "portaudio_static_x64"
    PATHS ${BUILD_PATH_LIB_RELEASE}/portaudio/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    PORTMIDI_LIB_RELEASE
    NAMES "portmidi"
    PATHS ${BUILD_PATH_LIB_RELEASE}/portmidi/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)

find_library(
    GLFW_LIB_DEBUG
    NAMES "glfw3" "glfw3dll"
    PATHS ${BUILD_PATH_LIB_DEBUG}/glfw/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    KISSFFT_LIB_DEBUG
    NAMES "kissfft-float"
    PATHS ${BUILD_PATH_LIB_DEBUG}/kissfft/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    SQLITECPP_LIB_DEBUG
    NAMES "SQLiteCpp"
    PATHS ${BUILD_PATH_LIB_DEBUG}/SQLiteCpp/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    SQLITE3_LIB_DEBUG
    NAMES "sqlite3"
    PATHS ${BUILD_PATH_LIB_DEBUG}/SQLiteCpp/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
string(TOLOWER "soxr-${CMAKE_CXX_COMPILER_ID}-debug" SOXR_DEBUG_DLL_NAME)
find_library(
    SOXR_LIB_DEBUG
    NAMES ${SOXR_DEBUG_DLL_NAME}
    PATHS ${BUILD_PATH_LIB_DEBUG}/soxr/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    PORTAUDIO_LIB_DEBUG
    NAMES "portaudio" "portaudio.dll" "portaudio_static_x64"
    PATHS ${BUILD_PATH_LIB_DEBUG}/portaudio/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)
find_library(
    PORTMIDI_LIB_DEBUG
    NAMES "portmidi"
    PATHS ${BUILD_PATH_LIB_DEBUG}/portmidi/
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
    REQUIRED)

# Need both versions to make CMake happy
if(NOT GLFW_LIB_DEBUG)
    message(FATAL_ERROR "glfw3 not found in ${GLFW_LIB_DEBUG}/glfw/lib")
endif()
if(NOT GLFW_LIB_RELEASE)
    message(FATAL_ERROR "glfw3 not found in ${BUILD_PATH_LIB_RELEASE}/glfw/lib")
endif()



# pybind is header only and identical in release and debug.
# it cannot be used out of the box and has to be installed.
# for simplicity purposes we just install it in both release and debug and check for presence of either.
set(PYBIND11_CPP_STANDARD -std=c++17)
find_package(pybind11 REQUIRED PATHS "${BUILD_PATH_LIB_DEBUG}/pybind11" "${BUILD_PATH_LIB_RELEASE}/pybind11") 

find_package(Threads REQUIRED )

if (LINUX)
    find_package(X11 REQUIRED)
    find_package(ALSA REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
endif(LINUX)

if (APPLE)
    find_library(APPLE_FOUNDATION_LIBRARY Foundation REQUIRED)
    find_library(APPLE_FOUNDATION_LIBRARY CoreFoundation REQUIRED)
    find_library(APPLE_COREAUDIO_LIBRARY CoreAudio REQUIRED)
    find_library(APPLE_AUDIOTOOLBOX_LIBRARY AudioToolbox REQUIRED)
    find_library(APPLE_AUDIOUNIT_LIBRARY AudioUnit REQUIRED)
    find_library(APPLE_CARBON_LIBRARY Carbon REQUIRED)
    find_library(APPLE_COCOA_LIBRARY Cocoa REQUIRED)
    find_library(APPLE_IOKIT_LIBRARY IOKit REQUIRED)
    find_library(APPLE_OPENGL_LIBRARY OpenGL REQUIRED)
    find_library(APPLE_COREVIDEO_LIBRARY CoreVideo REQUIRED)
    find_library(APPLE_COREMIDI_LIBRARY CoreMidi REQUIRED)

endif(APPLE)



FUNCTION(SET_TARGET_PROJECT_INCLUDE_DIRS TARGETNAME)
    if (LINUX)
        target_link_directories(${TARGETNAME} PUBLIC ${GTK3_LIBRARY_DIRS})
        target_compile_definitions(${TARGETNAME} PUBLIC ${GTK3_CFLAGS_OTHER})
        target_include_directories(${TARGETNAME} SYSTEM PUBLIC 
            ${GTK3_INCLUDE_DIRS}
            ${X11_X11_INCLUDE_PATH}
            ${ALSA_INCLUDE_DIR})
    endif(LINUX)

    target_include_directories(${TARGETNAME} PUBLIC "${MAIN_SRC_PATH}")
    target_include_directories(${TARGETNAME} PUBLIC "${MAIN_SRC_PATH}/include")
    target_include_directories(${TARGETNAME} SYSTEM PUBLIC 
        "${MAIN_SRC_PATH}/thirdparty"
        "${MAIN_SRC_PATH}/nanovg")
    target_include_directories(${TARGETNAME} SYSTEM PUBLIC
        ${PROJECT_DEPS_INSTALL_PATH}/${LIB_GN_EXPR}/glfw/include
        ${PROJECT_DEPS_INSTALL_PATH}/${LIB_GN_EXPR}/SQLiteCpp/include
        ${PROJECT_DEPS_INSTALL_PATH}/${LIB_GN_EXPR}/soxr/include
        ${PROJECT_DEPS_INSTALL_PATH}/${LIB_GN_EXPR}/portaudio/include
        ${PROJECT_DEPS_INSTALL_PATH}/${LIB_GN_EXPR}/portmidi/include
        ${PROJECT_DEPS_INSTALL_PATH}/${LIB_GN_EXPR}/kissfft/include
        ${PROJECT_DEPS_INSTALL_PATH}/${LIB_GN_EXPR}/pybind11/include
        ${PROJECT_DEPS_PATH}/glm
        ${PROJECT_DEPS_PATH}/SplineLibrary/spline_library
        ${PROJECT_DEPS_PATH}/cereal/include
        ${PROJECT_DEPS_PATH}/kissfft)

    if (PROJECT_CFG_USE_OPENGL3)
        # use OpenGL 3.2 core profile headers to avoid accidental use of legacy or forward
        target_include_directories(${TARGETNAME} SYSTEM PUBLIC ${PROJECT_DEPS_PATH}/glad/gl-3.2-core/include)
    else()
        # use OpenGL 3.2 compatibility profile headers
        target_include_directories(${TARGETNAME} SYSTEM PUBLIC ${PROJECT_DEPS_PATH}/glad/gl-3.2-compat/include)
    endif()
ENDFUNCTION()