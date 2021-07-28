if(NOT CMAKE_BUILD_TYPE) 
    set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)
if(UNIX AND NOT APPLE)
set(LINUX TRUE)
endif()

FUNCTION(PREPEND var prefix)
  SET(listVar "")
  FOREACH(f ${ARGN})
    LIST(APPEND listVar "${prefix}${f}")
  ENDFOREACH(f)
  SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

FUNCTION(ADD_POST_BUILD_COMMANDS targetBuildName)
  if (IS_MINGW_BUILD)
    if (CV2PDB) 
      add_custom_command(
          TARGET ${targetBuildName} POST_BUILD
          COMMAND ${CV2PDB} -k $<TARGET_FILE:${targetBuildName}>
          COMMENT "Generating PDB for MinGW binary"
      )
    else()
      message(STATUS "Configured without CV2PDB. Skipping PDB generation")
      add_custom_command(
          TARGET ${targetBuildName} POST_BUILD
          COMMENT "Configured without CV2PDB. Skipping PDB generation"
      )
    endif()
  endif()
ENDFUNCTION()
# macro to add define a executable binary to be built
FUNCTION(SET_APP_BUILD appname)
  set(tmp1 "${appname}-${BUILD_BINARY_SUFFIX}")
  set_target_properties(${appname} PROPERTIES OUTPUT_NAME ${tmp1})
  set_target_properties(${appname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${APP_WORKING_DIR}$<$<CONFIG:Debug>:>)
  set_target_properties(${appname} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${APP_WORKING_DIR})
  set_target_properties(${appname} PROPERTIES OUTPUT_NAME_DEBUG "${appname}-${CMAKE_CXX_COMPILER_ID}-debug${OUTPUT_BINARY_SUFFIX}" )
  set_target_properties(${appname} PROPERTIES OUTPUT_NAME_RELEASE "${appname}-${CMAKE_CXX_COMPILER_ID}-release${OUTPUT_BINARY_SUFFIX}" )
  set_target_properties(${appname} PROPERTIES OUTPUT_NAME_MINSIZEREL "${appname}-${CMAKE_CXX_COMPILER_ID}-minsizerel${OUTPUT_BINARY_SUFFIX}" )
  set_target_properties(${appname} PROPERTIES OUTPUT_NAME_RELWITHDEBINFO "${appname}-${CMAKE_CXX_COMPILER_ID}-relwithdebinfo${OUTPUT_BINARY_SUFFIX}" )
ENDFUNCTION(SET_APP_BUILD)

FUNCTION(GENERATE_BUILDINFO_CPP)
  # process variables from CMAKE build system towards a buildinfo.cpp file
  # this only happens at configuration and requires manual deletion of buildinfo.cpp to reflect the latest options
  get_property(BUILD_COMPILE_OPTIONS        DIRECTORY  PROPERTY COMPILE_OPTIONS)
  get_property(BUILD_COMPILE_DEFS           DIRECTORY  PROPERTY COMPILE_DEFINITIONS)
  get_property(BUILD_CXX_STANDARD           TARGET ${BUILD_NAME}  PROPERTY CXX_STANDARD)
  string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_SUFFIX)
  separate_arguments(GLOBAL_FLAGS UNIX_COMMAND "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BUILD_TYPE_SUFFIX}}")
  list(APPEND BUILD_COMPILE_OPTIONS ${GLOBAL_FLAGS})
  list(APPEND BUILD_COMPILE_OPTIONS -std=c++${BUILD_CXX_STANDARD})
  string(REPLACE ";" " " BUILD_COMPILE_OPTIONS "${BUILD_COMPILE_OPTIONS}")
  string(REPLACE ";" " -D" BUILD_COMPILE_DEFS "-D${BUILD_COMPILE_DEFS}")
  CONFIGURE_FILE( ${DAW_SRC_PATH}/app/buildinfo.cpp.in ${CMAKE_BINARY_DIR}/buildinfo.cpp ESCAPE_QUOTES)
ENDFUNCTION()

if (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
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

find_path(DAW_DEPS_PATH PATHS ${DAW_DEPS_PATH})
if(NOT DAW_DEPS_PATH)
  message(FATAL_ERROR "Can't find DAW_DEPS_PATH")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
OPTION(DEBUG_STD_LIB "Enable standard library assertions" OFF) # Enabled by default

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  add_compile_options(-Wall -Wno-inconsistent-missing-override -Wno-unused-parameter) # -Wshadow sadly no working option for warning on shadow local only
  #add_compile_options(-ftime-trace) # profile compilation times
  if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")

  #address sanitizer: Disable ADD_POST_BUILD_COMMANDS and set ASAN_SYMBOLIZER_PATH=C:\dev\llvm-mingw-ca329c1-full\bin\llvm-symbolizer.exe
    #add_compile_options(-fsanitize=address)
    #add_link_options(-fsanitize=address)

    # add_compile_definitions(_GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC ENABLE_MICHAELS_GLIBCXX_HACKS)
    if (DEBUG_STD_LIB)
      add_compile_definitions(_GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC)
      add_compile_definitions(_LIBCPP_DEBUG)
    endif (DEBUG_STD_LIB)
    add_definitions(-D_LIBCPP_NO_EXCEPTIONS) 
  endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-Wall -Wno-sign-compare)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX /wd4067 /wd4267 /wd4244)
  add_definitions(/MP14)
endif()
set(IS_MINGW_BUILD OFF)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  # can't use stack-protector with clang/gnu toolchain on windows
  if (NOT WIN32)  
    # stack protector improves debugging corrupted stacks on linux
    add_compile_options(-fstack-protector)
  else() 
    set(IS_MINGW_BUILD ON)
    add_compile_options(-gdwarf-3) #cv2pdb does not produce correct filenames/lines with dwarf > 3
  #  add_compile_definitions(_MSVCRT_VERSION__=0x1200) #link against msvcr120 runtime
  endif()
  # improve debugging
  add_compile_options(-fno-omit-frame-pointer)
endif()

if (IS_MINGW_BUILD)
  find_program(CV2PDB cv2pdb)
endif()

#SET(APP_COMMON_FLAGS VST_FORCE_DEPRECATED=0 USE_GLAD_GL_HEADERS GLM_FORCE_CXX14=1 PA_ENABLE_DEBUG_OUTPUT=1)
SET(APP_COMMON_FLAGS VST_FORCE_DEPRECATED=0 USE_GLAD_GL_HEADERS GLM_FORCE_CXX14=1)
if (WIN32)
  SET(APP_COMMON_FLAGS ${APP_COMMON_FLAGS} PA_USE_ASIO=1 PA_USE_DS=1 PA_USE_WASAPI=1 PA_USE_WDMKS=1)
endif(WIN32)
if (UNIX)
  SET(APP_COMMON_FLAGS ${APP_COMMON_FLAGS} PA_USE_ALSA=1)
	set(APP_COMMON_FLAGS ${APP_COMMON_FLAGS} __cdecl="")
endif(UNIX)

FOREACH(app_common_flag ${APP_COMMON_FLAGS})
  message(STATUS "Define: ${app_common_flag}")
  add_definitions(-D${app_common_flag})
ENDFOREACH(app_common_flag)

