
# macro to set common properties on an executable
FUNCTION(CONFIGURE_TARGET_OUTPUT buildtarget)
  # As per CMake docs: Add empty generator expr to avoid a configuration subdirectory on multi configs
  set_target_properties(${buildtarget} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ABS_WORKING_DIR}$<0:...>)
  set_target_properties(${buildtarget} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ABS_WORKING_DIR})
  set_target_properties(${buildtarget} PROPERTIES OUTPUT_NAME "${buildtarget}-${BUILD_BINARY_SUFFIX}")
  if (NOT WIN32)
    set_target_properties(${buildtarget} PROPERTIES LINK_FLAGS "-Wl,--build-id=0x${GIT_SHA1}")
  endif()
  if (NOT MSVC AND PROJECT_GENERATE_LINKER_MAP) 
    set_target_properties(${buildtarget} PROPERTIES LINK_FLAGS "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${buildtarget}_linker.map")
  endif()
  set(TARGET_VERSION "0.0.0.0")
  if (NOT ${PROJECT_VERSION} STREQUAL "")
    set(TARGET_VERSION ${PROJECT_VERSION})
  else()
    set(TARGET_VERSION ${CMAKE_PROJECT_VERSION})
  endif()
  if (NOT ${TARGET_VERSION} STREQUAL "")
    set_target_properties(${buildtarget} PROPERTIES VERSION "${TARGET_VERSION}")
  endif()
  if (WIN32 AND NOT PROJECT_NO_WINDRES)
    string(REPLACE "." "," WIN_EXE_VERSION "${TARGET_VERSION}")
    set(VER_FILEVERSION ${WIN_EXE_VERSION})
    set(VER_FILEVERSION_STR "${TARGET_VERSION}")
    set(VER_FILDESCRIPTION_STR "git ${GIT_SHA1_DIRTY}")
    set(VER_PRODUCTVERSION ${WIN_EXE_VERSION})
    set(VER_PRODUCTVERSION_STR "${TARGET_VERSION}")
    set(VER_PRODUCTNAME_STR "${PROJECT_NAME}")
    set(VER_FILENAME_STR "${PROJECT_NAME}.exe")
    set(VER_COPYRIGHT_STR "(c) Michael Hept")
    configure_file(
      "${MAIN_SRC_PATH}/version.rc.in"
      "${CMAKE_CURRENT_BINARY_DIR}/${buildtarget}_version.rc"
      NEWLINE_STYLE LF
    )
    target_sources(${buildtarget} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${buildtarget}_version.rc")
  endif()
  if (NOT MSVC) 
    add_custom_command(
      TARGET ${buildtarget} POST_BUILD
      DEPENDS ${buildtarget}
      COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}>
      ARGS --strip-all $<TARGET_FILE:${buildtarget}>
    )
  endif()
ENDFUNCTION(CONFIGURE_TARGET_OUTPUT)

FUNCTION(GENERATE_TARGET_BUILD_INFO buildtarget)
  file (GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/${buildtarget}_buildinfo_$<CONFIG>.cpp" 
    CONTENT
"#ifndef __TIMESTAMP__
#define __TIMESTAMP__ \"Undefined\"
#endif
namespace BuildInfo {
  const char* GIT_SHA1             = \"${GIT_SHA1_DIRTY}\";
  const char* COMPILE_OPTIONS      = \"$<JOIN:${CMAKE_CXX_FLAGS};$<IF:$<CONFIG:Debug>,${CMAKE_CXX_FLAGS_DEBUG},${CMAKE_CXX_FLAGS_RELEASE}>;$<TARGET_PROPERTY:${buildtarget},COMPILE_OPTIONS>, >\";
  const char* COMPILE_DEFS         = \"$<JOIN:$<TARGET_PROPERTY:${buildtarget},COMPILE_DEFINITIONS>, >\";
  const char* COMPILER_ID          = \"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\";
  const char* COMPILER_PATH        = \"${CMAKE_CXX_COMPILER}\";
  const char* BUILD_BINARY_VERSION = \"${CMAKE_PROJECT_VERSION}\";
  const char* BUILD_BINARY_NAME    = \"Daw-${BUILD_BINARY_SUFFIX}\";
  const char* BUILD_TIMESTAMP      = __TIMESTAMP__;
} // namespace BuildInfo"
    NEWLINE_STYLE LF
  )
  target_sources(${buildtarget} PRIVATE "${CMAKE_BINARY_DIR}/${buildtarget}_buildinfo_$<CONFIG>.cpp")
ENDFUNCTION()
