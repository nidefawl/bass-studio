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
    "C:/dev/daw-deps"
  NO_DEFAULT_PATH 
  REQUIRED)

if (DEFINED DEPS_BUILD_FOLDER)
  list(APPEND DEPS_SEARCH_PATHS ${DEPS_BUILD_FOLDER})
endif()
list(APPEND DEPS_SEARCH_PATHS 
  "${CMAKE_SOURCE_DIR}/../build-deps/install"
)

find_path(PROJECT_DEPS_INSTALL_PATH
  NAMES
    "include"
    "lib"
  PATHS 
    ${DEPS_SEARCH_PATHS}
  NO_DEFAULT_PATH
  REQUIRED
)
message(STATUS "PROJECT_DEPS_INSTALL_PATH ${PROJECT_DEPS_INSTALL_PATH}")

# MinSizeRel may link against debug libraries (depends on order of definition inside imported library) 
# Force linkage against Release if no explicit import target for config is provided
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL "MinSizeRel;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;")
add_subdirectory("${PROJECT_DEPS_PATH}/slowstacktrace" "libstracktrace")
find_library(ZLIB_RELEASE PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH NAMES "zlibstatic_release" "z_release" "zlib_release" "zlib_release.dll" PATH_SUFFIXES lib)
find_library(ZLIB_DEBUG PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH NAMES "zlibstaticd" "z_debug" "zlibstatic_debug" "zlibd" "zlib_debug.dll" PATH_SUFFIXES lib)
message(STATUS "ZLIB_RELEASE ${ZLIB_RELEASE}")
message(STATUS "ZLIB_DEBUG ${ZLIB_DEBUG}")
add_subdirectory("${PROJECT_DEPS_PATH}/libarchive-cmake-build" "libarchive")
find_package(Threads REQUIRED)


find_package(glfw3      PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)
find_package(SQLiteCpp  PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)
find_package(PortAudio  PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)
find_package(PortMidi   PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)
find_package(kissfft    PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)

find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11   PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH) 

string(TOLOWER "soxr-${CMAKE_CXX_COMPILER_ID}-release" SOXR_DYNLIB_NAME)
find_library(SOXR_LIB   PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH NAMES ${SOXR_DYNLIB_NAME} PATH_SUFFIXES lib)
message(STATUS "SOXR_LIB ${SOXR_LIB}")


if (LINUX)
  find_package(X11 REQUIRED)
  find_package(ALSA REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(DBUS REQUIRED dbus-1)
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

FUNCTION(CONFIGURE_TARGET_DEPS TARGETNAME)
  if (LINUX)
    target_link_libraries(${TARGETNAME} PUBLIC ${DBUS_LIBRARIES})
    target_include_directories(${TARGETNAME} SYSTEM PUBLIC 
        ${DBUS_INCLUDE_DIRS}
        ${X11_X11_INCLUDE_PATH}
        ${ALSA_INCLUDE_DIR})
  endif(LINUX)
  
  target_include_directories(${TARGETNAME} PUBLIC ${MAIN_SRC_PATH})
  target_include_directories(${TARGETNAME} PUBLIC ${MAIN_SRC_PATH}/include)
  target_include_directories(${TARGETNAME} SYSTEM PUBLIC 
    ${MAIN_SRC_PATH}/thirdparty
    ${MAIN_SRC_PATH}/nanovg
  )
  target_include_directories(${TARGETNAME} SYSTEM PUBLIC
    ${PROJECT_DEPS_PATH}/glm
    ${PROJECT_DEPS_PATH}/SplineLibrary/spline_library
    ${PROJECT_DEPS_PATH}/cereal/include
    ${PROJECT_DEPS_PATH}/clap/include
    ${PROJECT_DEPS_PATH}/clap-helpers/include
    ${PROJECT_DEPS_PATH}/dr_libs
  )
  target_include_directories(${TARGETNAME} SYSTEM PUBLIC ${PROJECT_DEPS_PATH}/muparser/include)

  if (PROJECT_CFG_USE_OPENGL3)
    # use OpenGL 3.2 core profile headers to avoid accidental use of legacy or forward
    target_include_directories(${TARGETNAME} SYSTEM PUBLIC ${PROJECT_DEPS_PATH}/glad/gl-3.2-core/include)
  else()
    # use OpenGL 3.2 compatibility profile headers
    target_include_directories(${TARGETNAME} SYSTEM PUBLIC ${PROJECT_DEPS_PATH}/glad/gl-3.2-compat/include)
  endif()
ENDFUNCTION()