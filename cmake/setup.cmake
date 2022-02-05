
set(LINUX FALSE)
if(UNIX AND NOT APPLE)
  set(LINUX TRUE)
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

if(NOT MSVC AND NOT CMAKE_BUILD_TYPE) 
  message(WARNING "CMAKE_BUILD_TYPE not specified: Setting Debug")
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(OUTPUT_BINARY_SUFFIX "" CACHE STRING "OUTPUT_BINARY_SUFFIX")
set(BUILD_BINARY_SUFFIX "${CMAKE_CXX_COMPILER_ID}-$<LOWER_CASE:$<CONFIG>>${OUTPUT_BINARY_SUFFIX}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CLANG)
  OPTION(DEBUG_STD_LIB "Enable standard library assertions" OFF) # Enabled by default
  #add_compile_options(-ftime-trace) # profile compilation times
  if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")

    #address sanitizer: Disable ADD_POST_BUILD_COMMANDS and set ASAN_SYMBOLIZER_PATH=C:\dev\llvm-mingw-ca329c1-full\bin\llvm-symbolizer.exe
    #add_compile_options(-fsanitize=address)
    #add_link_options(-fsanitize=address)
    if (DEBUG_STD_LIB)
      add_compile_definitions(_GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC)
      add_compile_definitions(_LIBCPP_DEBUG)
    endif (DEBUG_STD_LIB)

    add_compile_definitions(_LIBCPP_NO_EXCEPTIONS) 

  endif()
elseif (MSVC)
    add_compile_options(/external:anglebrackets /external:W0)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX) 
    add_compile_options(/MP16)
endif()


# if (LINUX OR (WIN32 AND NOT MSVC)) 
if (LINUX) 
    if (WIN32)
        set(WIN32_USE_STACK_PROTECTOR ON)
    endif()
    # stack protector improves debugging corrupted stacks on linux
    add_compile_options(-fstack-protector)
else()
    set(WIN32_USE_STACK_PROTECTOR OFF)
endif()
message(STATUS "WIN32_USE_STACK_PROTECTOR ${WIN32_USE_STACK_PROTECTOR}")

if (IS_MINGW_BUILD)
  # improve debugging
  add_compile_options(-fno-omit-frame-pointer)
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


FUNCTION(PREPEND var prefix)
  SET(listVar "")
  FOREACH(f ${ARGN})
    LIST(APPEND listVar "${prefix}${f}")
  ENDFOREACH(f)
  SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

FUNCTION(ADD_POST_BUILD_COMMANDS targetBuildName)
  if (IS_MINGW_BUILD AND CV2PDB)
    add_custom_command(
        TARGET ${targetBuildName} POST_BUILD
        COMMAND ${CV2PDB} -k $<TARGET_FILE:${targetBuildName}>
        COMMENT "Generating PDB for MinGW binary"
    )
  endif()
ENDFUNCTION()

# macro to add define a executable binary to be built
FUNCTION(SET_APP_BUILD appname)
  # As per CMake docs: Add empty generator expr to avoid a configuration subdirectory on multi configs
  set_target_properties(${appname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${APP_WORKING_DIR}$<0:...>)
  set_target_properties(${appname} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${APP_WORKING_DIR})
  set_target_properties(${appname} PROPERTIES OUTPUT_NAME "${appname}-${BUILD_BINARY_SUFFIX}")
ENDFUNCTION(SET_APP_BUILD)

FUNCTION(GENERATE_BUILDINFO_CPP)
  # process variables from CMAKE build system towards a buildinfo.cpp file
  # this only happens at configuration and requires manual deletion of buildinfo.cpp to reflect the latest options
  get_property(BUILDINFO_COMPILE_OPTIONS        DIRECTORY  PROPERTY COMPILE_OPTIONS)
  get_property(BUILDINFO_COMPILE_DEFS           DIRECTORY  PROPERTY COMPILE_DEFINITIONS)
  get_property(BUILD_CXX_STANDARD           TARGET ${BUILD_NAME}  PROPERTY CXX_STANDARD)
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
  CONFIGURE_FILE( ${DAW_SRC_PATH}/app/buildinfo.cpp.in ${CMAKE_BINARY_DIR}/buildinfo.cpp ESCAPE_QUOTES)
ENDFUNCTION()
