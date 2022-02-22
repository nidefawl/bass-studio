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
        "C:/dev/daw-deps"
        "D:/dev/daw-deps"
    REQUIRED)

message(STATUS "PROJECT_DEPS_PATH ${PROJECT_DEPS_PATH}")

find_path(PROJECT_DEPS_INSTALL_PATH
    NAMES
        "glfw"
        "portaudio"
        "portmidi"
        "soxr"
        "glm"
    PATHS 
        "${DEPS_BUILD_FOLDER}"
        "${CMAKE_SOURCE_DIR}/../build-deps/install"
        "${CMAKE_BINARY_DIR}/../build-deps/install"
    REQUIRED
)

find_package(Threads REQUIRED)

find_package(glfw3      PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)
find_package(SQLiteCpp  PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)
find_package(PortAudio  PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)
find_package(PortMidi   PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)
find_package(kissfft    PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)

set(PYBIND11_CPP_STANDARD -std=c++17)
find_package(pybind11   PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH) 

string(TOLOWER "soxr-${CMAKE_CXX_COMPILER_ID}-release" SOXR_DYNLIB_NAME)
find_library(SOXR_LIB   PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH NAMES ${SOXR_DYNLIB_NAME} PATH_SUFFIXES lib)
message(STATUS "SOXR_LIB ${SOXR_LIB}")


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
        target_compile_options(${TARGETNAME} PUBLIC ${GTK3_CFLAGS_OTHER})
        target_include_directories(${TARGETNAME} SYSTEM PUBLIC 
            ${GTK3_INCLUDE_DIRS}
            ${X11_X11_INCLUDE_PATH}
            ${ALSA_INCLUDE_DIR})
    endif(LINUX)

    target_include_directories(${TARGETNAME} PUBLIC ${MAIN_SRC_PATH})
    target_include_directories(${TARGETNAME} PUBLIC ${MAIN_SRC_PATH}/include)
    target_include_directories(${TARGETNAME} SYSTEM PUBLIC 
        ${MAIN_SRC_PATH}/thirdparty
        ${MAIN_SRC_PATH}/nanovg)
    target_include_directories(${TARGETNAME} SYSTEM PUBLIC
        ${PROJECT_DEPS_PATH}/glm
        ${PROJECT_DEPS_PATH}/SplineLibrary/spline_library
        ${PROJECT_DEPS_PATH}/cereal/include
    )

    if (PROJECT_CFG_USE_OPENGL3)
        # use OpenGL 3.2 core profile headers to avoid accidental use of legacy or forward
        target_include_directories(${TARGETNAME} SYSTEM PUBLIC ${PROJECT_DEPS_PATH}/glad/gl-3.2-core/include)
    else()
        # use OpenGL 3.2 compatibility profile headers
        target_include_directories(${TARGETNAME} SYSTEM PUBLIC ${PROJECT_DEPS_PATH}/glad/gl-3.2-compat/include)
    endif()
ENDFUNCTION()