
message(STATUS "CMake version: ${CMAKE_VERSION}")
if(NOT DEFINED PROJECT_NAME) 
  message(WARNING "Compiler not supported")
endif()

# Don't use -rdynamic
if (POLICY CMP0065)
  cmake_policy(SET CMP0065 NEW)
endif ()

set(PROJECT_SRC_PATH "${CMAKE_SOURCE_DIR}/src" CACHE PATH "Project source directory")
set(PROJECT_WORKING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/run" CACHE PATH "working directory (run)")
get_filename_component(ABS_WORKING_DIR "${PROJECT_WORKING_DIR}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}") # CMAKE_SOURCE_DIR for out of source builds?!

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

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
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX) 
  add_compile_options(/external:anglebrackets /external:W0)
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
  elseif (PROJECT_CFG_DEBUG_STD_LIB)
    add_compile_definitions(_GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC _LIBCPP_DEBUG)
  endif()
  if (CLANG)
    add_compile_definitions(_LIBCPP_NO_EXCEPTIONS)  
    add_compile_options(-ffunction-sections -fdata-sections)
  endif()
  # add_compile_options(-ftime-trace) # profile compilation times
  # address sanitizer: 
  # Disable ADD_POST_BUILD_COMMANDS and set ASAN_SYMBOLIZER_PATH=path\to\bin\llvm-symbolizer
  # add_compile_options(-fsanitize=address)
  # add_link_options(-fsanitize=address)

  # march=native optimization. Turn off for dist build
  add_compile_options(-march=native -mtune=native)
  add_link_options(-Wl,--gc-sections)
endif()

if (NOT MSVC AND CLANG)
  add_compile_options(-fcolor-diagnostics -fansi-escape-codes)  
  add_link_options(-fcolor-diagnostics -fansi-escape-codes)
endif()

# if (MINGW AND CLANG)
#   add_compile_options($<$<CONFIG:Debug,RelWithDebInfo>:-gcodeview>)
#   add_link_options($<$<CONFIG:Debug,RelWithDebInfo>:-Wl,-pdb=>)
# endif()
