
if(NOT CMAKE_BUILD_TYPE) 
    set(CMAKE_BUILD_TYPE Debug)
endif(NOT CMAKE_BUILD_TYPE)

FUNCTION(PREPEND var prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)
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

find_path(DAW_DEPS_PATH PATHS ${DAW_DEPS_PATH})
if(NOT DAW_DEPS_PATH)
  message(FATAL_ERROR "Can't find DAW_DEPS_PATH")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(DAW_COMPILE_FLAGS -DVST_FORCE_DEPRECATED=0 -DUSE_GLAD_GL_HEADERS -DGLM_FORCE_CXX14=1 -DNO_LEAK_DETECT -DPA_ENABLE_DEBUG_OUTPUT=1)
if (WIN32)
  SET(DAW_COMPILE_FLAGS ${DAW_COMPILE_FLAGS} -DPA_USE_ASIO=1 -DPA_USE_DS=1 -DPA_USE_WASAPI=1 -DPA_USE_WDMKS=1)
endif(WIN32)
if (UNIX)
  SET(DAW_COMPILE_FLAGS ${DAW_COMPILE_FLAGS} -DPA_USE_ALSA=1)
	set(DAW_COMPILE_FLAGS ${DAW_COMPILE_FLAGS} -D__cdecl="")
endif(UNIX)


if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  add_compile_options(-Wall -Wno-inconsistent-missing-override)
  if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    #add_definitions(-D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC)
  endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_compile_options(-Wall -Wno-sign-compare)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -DNOMINMAX /wd4067 /wd4267 /wd4244)
endif()
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  if (NOT WIN32)  #cant use stack-protector with clang/gnu toolchain on windows
  	add_compile_options(-fstack-protector)
  endif(NOT WIN32)
  add_compile_options(-fno-omit-frame-pointer)
endif()




add_definitions(${DAW_COMPILE_FLAGS})
