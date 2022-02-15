
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

if (MSVC)
    add_compile_options(/external:anglebrackets /external:W0)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX) 
    add_compile_options(/MP7)
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

if (NOT MSVC)
  set(NO_TEMP_OBJECT_A On)
  # By default cmake generates a temporary object.a archive on windows-gnu 
  # Resetting the link rules here avoids this step and saves significant time when linking
  if (WIN32 AND NO_TEMP_OBJECT_A) 
    message(STATUS "NO TEMP OBJECT")
    foreach(lang C CXX)
      set(CMAKE_${lang}_CREATE_SHARED_MODULE
      "<CMAKE_${lang}_COMPILER> <CMAKE_SHARED_MODULE_${lang}_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_MODULE_CREATE_${lang}_FLAGS> -o <TARGET> ${CMAKE_GNULD_IMAGE_VERSION} <OBJECTS> <LINK_LIBRARIES>")
      set(CMAKE_${lang}_CREATE_SHARED_LIBRARY
      "<CMAKE_${lang}_COMPILER> <CMAKE_SHARED_LIBRARY_${lang}_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS> -o <TARGET> -Wl,--out-implib,<TARGET_IMPLIB> ${CMAKE_GNULD_IMAGE_VERSION} <OBJECTS> <LINK_LIBRARIES>")
      set(CMAKE_${lang}_LINK_EXECUTABLE
      "<CMAKE_${lang}_COMPILER> <FLAGS> <CMAKE_${lang}_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> -Wl,--out-implib,<TARGET_IMPLIB> ${CMAKE_GNULD_IMAGE_VERSION} <LINK_LIBRARIES>")
    endforeach()
  endif()
endif()

if (IS_MINGW_BUILD)
  find_program(CV2PDB cv2pdb)
  if (NOT CV2PDB) 
    message(STATUS "CV2PDB not found. Skipping PDB generation")
  else() 
    message(STATUS "CV2PDB found")
    add_compile_options(-gdwarf-3) #cv2pdb does not produce correct filenames/lines with dwarf > 3
  endif()
endif()

FUNCTION(ADD_POST_BUILD_COMMANDS targetBuildName)
  if (IS_MINGW_BUILD AND CV2PDB)
    add_custom_command(
        TARGET ${targetBuildName} POST_BUILD
        COMMAND ${CV2PDB} -k $<TARGET_FILE:${targetBuildName}>
        COMMENT "Generating PDB for MinGW binary"
    )
  endif()
ENDFUNCTION()

# macro to set common properties on an executable
FUNCTION(SET_APP_BUILD appname)
  # As per CMake docs: Add empty generator expr to avoid a configuration subdirectory on multi configs
  set_target_properties(${appname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_WORKING_DIR}$<0:...>)
  set_target_properties(${appname} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_WORKING_DIR})
  set_target_properties(${appname} PROPERTIES OUTPUT_NAME "${appname}-${BUILD_BINARY_SUFFIX}")
ENDFUNCTION(SET_APP_BUILD)

FUNCTION(GENERATE_BUILDINFO_CPP TARGETNAME CPP_IN_FILE)
  # process variables from CMAKE build system towards a buildinfo.cpp file
  # this only happens at configuration and requires manual deletion of buildinfo.cpp to reflect the latest options
  get_property(BUILDINFO_COMPILE_OPTIONS DIRECTORY       PROPERTY COMPILE_OPTIONS)
  get_property(BUILDINFO_COMPILE_DEFS    DIRECTORY       PROPERTY COMPILE_DEFINITIONS)
  get_property(BUILD_CXX_STANDARD TARGET ${TARGETNAME} PROPERTY CXX_STANDARD)
  if (CMAKE_BUILD_TYPE)
    string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_SUFFIX)
    separate_arguments(GLOBAL_FLAGS UNIX_COMMAND "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BUILD_TYPE_SUFFIX}}")
  else()
    separate_arguments(GLOBAL_FLAGS UNIX_COMMAND "${CMAKE_CXX_FLAGS}")
  endif()
  list(APPEND BUILDINFO_COMPILE_OPTIONS ${GLOBAL_FLAGS})
  list(APPEND BUILDINFO_COMPILE_OPTIONS -std=c++${BUILD_CXX_STANDARD})
  if (BUILDINFO_COMPILE_OPTIONS)
    string(REPLACE ";" " " BUILDINFO_COMPILE_OPTIONS "${BUILDINFO_COMPILE_OPTIONS}")
  endif()
  if (BUILDINFO_COMPILE_DEFS)
    string(REPLACE ";" " -D" BUILDINFO_COMPILE_DEFS "-D${BUILDINFO_COMPILE_DEFS}")
  endif()
  CONFIGURE_FILE( ${CPP_IN_FILE} ${CMAKE_BINARY_DIR}/buildinfo.cpp ESCAPE_QUOTES)
ENDFUNCTION()
