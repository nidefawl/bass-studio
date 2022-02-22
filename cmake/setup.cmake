
message(STATUS "CMake version: ${CMAKE_VERSION}")
if(NOT DEFINED PROJECT_NAME) 
  message(WARNING "Compiler not supported")
endif()

# Don't use -rdynamic
if (POLICY CMP0065)
    cmake_policy(SET CMP0065 NEW)
endif ()

set(PROJECT_SRC_PATH "${CMAKE_SOURCE_DIR}/src" CACHE PATH "Project source directory")
# instead of an installation step we only copy the executable to PROJECT_WORKING_DIR
set(PROJECT_WORKING_DIR "../run/" CACHE PATH "working directory (run)")
get_filename_component(ABS_WORKING_DIR "${PROJECT_WORKING_DIR}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}") # CMAKE_SOURCE_DIR for out of source builds?!

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
# if (NOT MSVC)
#     set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
#     set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")
# endif ()


set(LINUX FALSE)
if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
endif()

if (UNIX)
    set(PROJECT_PLATFORM "linux")
endif()
if (WIN32)
    set(PROJECT_PLATFORM "win")
endif()

set(IS_MINGW_BUILD OFF)
if (WIN32 AND NOT MSVC)
  set(IS_MINGW_BUILD ON)
endif()

set(CLANG FALSE)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CLANG TRUE)
endif()

if(NOT MSVC AND NOT CLANG) 
  message(WARNING "Compiler not supported")
endif()

set(OUTPUT_BINARY_SUFFIX "" CACHE STRING "OUTPUT_BINARY_SUFFIX")
set(BUILD_BINARY_SUFFIX "${CMAKE_CXX_COMPILER_ID}-$<LOWER_CASE:$<CONFIG>>${OUTPUT_BINARY_SUFFIX}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (WIN32)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX) 
endif()

if (MSVC)
    add_compile_options(/external:anglebrackets /external:W0)
    add_compile_options(/MP7)
#    add_compile_options($<IF:$<CONFIG:Debug>,,/Ob3>)
    add_compile_options($<IF:$<CONFIG:Debug>,,/Gy>)
    add_link_options($<IF:$<CONFIG:Debug>,,/INCREMENTAL:NO>)
    add_link_options($<IF:$<CONFIG:Debug>,,/OPT:REF>)
    add_link_options($<IF:$<CONFIG:Debug>,,/OPT:ICF>)
else()
    set(PROJECT_CFG_USE_STACK_PROTECTOR "OFF" CACHE STRING "Use fstack-protector (ON/OFF/DebugOnly)")
    set_property(CACHE PROJECT_CFG_USE_STACK_PROTECTOR PROPERTY STRINGS ON OFF DebugOnly)

    set(PROJECT_CFG_NO_OMIT_FRAME_POINTER "DebugOnly" CACHE STRING "Use fno-omit-frame-pointer (ON/OFF/DebugOnly)")
    set_property(CACHE PROJECT_CFG_NO_OMIT_FRAME_POINTER PROPERTY STRINGS ON OFF DebugOnly)

    set(PROJECT_CFG_DEBUG_STD_LIB "OFF" CACHE STRING "std-lib asserts + std::string debugging (ON/OFF/DebugOnly)")
    set_property(CACHE PROJECT_CFG_DEBUG_STD_LIB PROPERTY STRINGS ON OFF DebugOnly)

    # improve debugging
    if(PROJECT_CFG_USE_STACK_PROTECTOR STREQUAL "DebugOnly")
        add_compile_options($<$<CONFIG:Debug>:-fstack-protector>)
    elseif (PROJECT_CFG_USE_STACK_PROTECTOR)
        add_compile_options(-fstack-protector)
    endif()

    # improve debugging
    if(PROJECT_CFG_NO_OMIT_FRAME_POINTER STREQUAL "DebugOnly")
        add_compile_options($<$<CONFIG:Debug>:-fno-omit-frame-pointer>)
    elseif (PROJECT_CFG_NO_OMIT_FRAME_POINTER)
        add_compile_options(-fno-omit-frame-pointer)
    endif()

    # improve debugging
    if(PROJECT_CFG_DEBUG_STD_LIB STREQUAL "DebugOnly")
        add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_DEBUG>)
        add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_DEBUG_PEDANTIC>)
        add_compile_definitions($<$<CONFIG:Debug>:_GLIBCXX_DEBUG>)
    elseif (PROJECT_CFG_DEBUG_STD_LIB)
        add_compile_definitions(_GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC _LIBCPP_DEBUG)
    endif()

    add_compile_definitions(_LIBCPP_NO_EXCEPTIONS) 
    # add_compile_options(-ftime-trace) # profile compilation times
    # address sanitizer: 
    # Disable ADD_POST_BUILD_COMMANDS and set ASAN_SYMBOLIZER_PATH=path\to\bin\llvm-symbolizer
    # add_compile_options(-fsanitize=address)
    # add_link_options(-fsanitize=address)
endif()
if (NOT MSVC AND CLANG)
  add_compile_options(-fcolor-diagnostics -fansi-escape-codes)  
  add_link_options(-fcolor-diagnostics -fansi-escape-codes)
endif()
if (WIN32 AND NOT MSVC AND CLANG)
  add_compile_options($<$<CONFIG:Debug>:-gcodeview>)
  add_link_options($<$<CONFIG:Debug>:-Wl,-pdb=>)
endif()

FUNCTION(ADD_POST_BUILD_COMMANDS targetBuildName)
  # Nothing
ENDFUNCTION()

# macro to set common properties on an executable
FUNCTION(SET_APP_BUILD appname)
  # As per CMake docs: Add empty generator expr to avoid a configuration subdirectory on multi configs
  set_target_properties(${appname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ABS_WORKING_DIR}$<0:...>)
  set_target_properties(${appname} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ABS_WORKING_DIR})
  set_target_properties(${appname} PROPERTIES OUTPUT_NAME "${appname}-${BUILD_BINARY_SUFFIX}")
ENDFUNCTION(SET_APP_BUILD)

FUNCTION(GENERATE_BUILDINFO_CPP TARGETNAME CPP_IN_FILE)
  file (GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/buildinfo_$<CONFIG>.cpp" 
    CONTENT
"#ifndef __TIMESTAMP__
#define __TIMESTAMP__ \"Undefined\"
#endif
namespace BuildInfo {
    const char* COMPILE_OPTIONS      = \"$<JOIN:${CMAKE_CXX_FLAGS};$<IF:$<CONFIG:Debug>,${CMAKE_CXX_FLAGS_DEBUG},${CMAKE_CXX_FLAGS_RELEASE}>;$<TARGET_PROPERTY:${TARGETNAME},COMPILE_OPTIONS>, >\";
    const char* COMPILE_DEFS         = \"$<JOIN:$<TARGET_PROPERTY:${TARGETNAME},COMPILE_DEFINITIONS>, >\";
    const char* COMPILER_ID          = \"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\";
    const char* COMPILER_PATH        = \"${CMAKE_CXX_COMPILER}\";
    const char* BUILD_BINARY_VERSION = \"0.4.5.1\";
    const char* BUILD_BINARY_NAME    = \"Daw-${BUILD_BINARY_SUFFIX}\";
    const char* BUILD_TIMESTAMP      = __TIMESTAMP__;
} // namespace BuildInfo"
    NEWLINE_STYLE LF
  )

ENDFUNCTION()
