if (NOT PRODUCT_VERSION)
  set(PRODUCT_VERSION ${CMAKE_PROJECT_VERSION})
endif()
if (NOT PRODUCT_VERSION_MAJOR_MINOR_PATCH_REVISION)
  set(PRODUCT_VERSION_MAJOR_MINOR_PATCH_REVISION "${PRODUCT_VERSION}")
endif()
if (NOT PROJECT_PRODUCT_NAME)
  set(PROJECT_PRODUCT_NAME ${PROJECT_NAME})
endif()
if (NOT PROJECT_VENDOR_NAME)
  set(PROJECT_VENDOR_NAME "Unknown")
endif()
if (NOT PROJECT_BINARY_NAME)
  set(PROJECT_BINARY_NAME ${PROJECT_NAME})
endif()
if (NOT PRODUCT_HOST_NAME)
  set(PRODUCT_HOST_NAME "${PROJECT_NAME}")
endif()
if (NOT PRODUCT_URL_DOCS)
  set(PRODUCT_URL_DOCS "")
endif()
if (NOT PRODUCT_URL_VENDOR)
  set(PRODUCT_URL_VENDOR "")
endif()
if (NOT PRODUCT_COPYRIGHT)
  set(PRODUCT_COPYRIGHT "© ${PROJECT_VENDOR_NAME}")
  set(PRODUCT_COPYRIGHT "(c) ${PROJECT_VENDOR_NAME}")
endif()

# macro to set common properties on an executable
FUNCTION(CONFIGURE_TARGET_OUTPUT buildtarget outputname programDisplayName)
  # As per CMake docs: Add empty generator expr to avoid a configuration subdirectory on multi configs
  set_target_properties(${buildtarget} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ABS_WORKING_DIR}$<0:...>)
  set_target_properties(${buildtarget} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${ABS_WORKING_DIR}$<0:...>)
  set_target_properties(${buildtarget} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ABS_WORKING_DIR})
  set_target_properties(${buildtarget} PROPERTIES OUTPUT_NAME "${outputname}-${BUILD_BINARY_SUFFIX}")
  if (LINUX)
    set_target_properties(${buildtarget} PROPERTIES LINK_FLAGS "-Wl,--build-id=0x${GIT_SHA1}")
  endif()
  if (WIN32 AND NOT MSVC)
    set_target_properties(${buildtarget} PROPERTIES LINK_FLAGS "-mwindows")
  endif()
  if (NOT MSVC AND PROJECT_GENERATE_LINKER_MAP) 
    set_target_properties(${buildtarget} PROPERTIES LINK_FLAGS "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${buildtarget}_linker.map")
  endif()
  if (NOT ${PRODUCT_VERSION} STREQUAL "")
    set_target_properties(${buildtarget} PROPERTIES VERSION "${PRODUCT_VERSION}")
  endif()
  if (WIN32 AND NOT PROJECT_NO_WINDRES)
    string(REPLACE "." "," WIN_EXE_VERSION "${PRODUCT_VERSION_MAJOR_MINOR_PATCH_REVISION}")
    set(VER_FILEVERSION ${WIN_EXE_VERSION})
    set(VER_FILEVERSION_STR "${PRODUCT_VERSION}")
    set(VER_FILDESCRIPTION_STR "${programDisplayName} ${PRODUCT_VERSION}")
    set(VER_PRODUCTVERSION ${WIN_EXE_VERSION})
    set(VER_PRODUCTVERSION_STR "${PRODUCT_VERSION}")
    set(VER_PRODUCTNAME_STR "${programDisplayName}")
    set(VER_FILENAME_STR "${outputname}.exe")
    set(VER_COPYRIGHT_STR "${PRODUCT_COPYRIGHT}")

    configure_file(
      "${MAIN_SRC_PATH}/version.rc.in"
      "${CMAKE_CURRENT_BINARY_DIR}/${buildtarget}_version.rc"
      NEWLINE_STYLE LF
    )
    target_sources(${buildtarget} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${buildtarget}_version.rc")
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
  const char* GIT_SHA1             = R\"(${GIT_SHA1_DIRTY})\";
  const char* COMPILE_OPTIONS      = R\"($<JOIN:${CMAKE_CXX_FLAGS};$<IF:$<CONFIG:Debug>,${CMAKE_CXX_FLAGS_DEBUG},${CMAKE_CXX_FLAGS_RELEASE}>;$<TARGET_PROPERTY:${buildtarget},COMPILE_OPTIONS>, >)\";
  const char* COMPILE_DEFS         = R\"($<JOIN:$<TARGET_PROPERTY:${buildtarget},COMPILE_DEFINITIONS>, >)\";
  const char* COMPILER_ID          = R\"(${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})\";
  const char* COMPILER_PATH        = R\"(${CMAKE_CXX_COMPILER})\";
  const char* BUILD_BINARY_VERSION = R\"(${PRODUCT_VERSION})\";
  const char* BUILD_BINARY_NAME    = R\"(${PROJECT_BINARY_NAME})\";
  const char* BUILD_TIMESTAMP      = __TIMESTAMP__;
  const char* PRODUCT_VENDOR       = R\"(${PROJECT_VENDOR_NAME})\";
  const char* PRODUCT_URL_DOCS     = R\"(${PRODUCT_URL_DOCS})\";
  const char* PRODUCT_URL_VENDOR   = R\"(${PRODUCT_URL_VENDOR})\";
  const char* PRODUCT_NAME_DISPLAY = R\"(${PROJECT_PRODUCT_NAME})\";
  const char* PRODUCT_NAME_UPPER   = R\"($<UPPER_CASE:${PROJECT_BINARY_NAME}>)\";
  const char* PRODUCT_NAME_LOWER   = R\"($<LOWER_CASE:${PROJECT_BINARY_NAME}>)\";
  const char* PRODUCT_HOST_NAME    = R\"(${PROJECT_PRODUCT_NAME})\";
  const char* PRODUCT_COPYRIGHT    = R\"(${PRODUCT_COPYRIGHT})\";
} // namespace BuildInfo"
    NEWLINE_STYLE LF
  )
  target_sources(${buildtarget} PRIVATE "${CMAKE_BINARY_DIR}/${buildtarget}_buildinfo_$<CONFIG>.cpp")
ENDFUNCTION()
