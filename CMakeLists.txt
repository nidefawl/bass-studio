cmake_minimum_required(VERSION 3.3)
message (STATUS "CMake version: ${CMAKE_VERSION}")

set(DAW_SRC_PATH "${CMAKE_SOURCE_DIR}/src")
set(BUILD_NAME "DAW")
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${BUILD_NAME})

project(${BUILD_NAME} C CXX)
option(BUILD_HOST "Build daw" ON)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_SCANNER "Build plugin scanner" OFF)
option(BUILD_CMDLINE_HOST "Build command line host" OFF)
option(BUILD_GUI_TEST "Build test app" OFF)
option(BUILD_RGB_MASTER "Build rgb master" OFF)
option(USE_OPENGL3 "Use OpenGL 3.x instead of OpenGL 2" ON)

if (BUILD_TESTS OR BUILD_SCANNER OR BUILD_CMDLINE_HOST OR BUILD_GUI_TEST OR BUILD_RGB_MASTER)
    set(BUILD_DAW_LIB ON)
else()
    set(BUILD_DAW_LIB OFF)
endif()

#set(BUILD_DAW_LIB ON) #override

if (NOT MSVC)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")
endif()
include("setup.cmake")
add_compile_definitions("USE_WIN32_EXC_HOOKS")
add_compile_definitions("BUILD_VSTHOST=1")
add_compile_definitions("BUILD_EXTERNAL_PLUGIN=0")
if (USE_OPENGL3)
    add_compile_definitions("NANOVG_GL3=1")
else()
    add_compile_definitions("NANOVG_GL2=1")
endif()
if (BUILD_TESTS)
    add_compile_definitions("BUILD_TESTS")
endif()


file(GLOB_RECURSE HDR_ALL_FILES src/*.h)
file(GLOB_RECURSE CXX_ALL_FILES src/*.cxx)
file(GLOB_RECURSE CPP_ALL_FILES src/*.cpp)
set(CPP_ALL_FILES ${CXX_ALL_FILES} ${CPP_ALL_FILES})
file(GLOB_RECURSE C_SRC_FILES src/*.c)
list(APPEND HDR_FILES "")
list(APPEND CPP_SRC_FILES "")
list(APPEND MM_SRC_FILES "")
if (APPLE)
	file(GLOB_RECURSE MM_SRC_FILES src/*.mm)
endif (APPLE)

foreach(f ${HDR_ALL_FILES})
    if (f MATCHES ".*/tests/[^/]*$")
        continue()
    endif()
    if (UNIX)
      if (f MATCHES ".*/host/platform/win/[^/]*$")
          continue()
      endif()
      if (f MATCHES ".*/platform/win/[^/]*$")
          continue()
      endif()
      if (f MATCHES ".*/platform/mingw/[^/]*$")
          continue()
      endif()
      if (f MATCHES ".*/external_asio.cpp$")
          continue()
      endif()
    endif (UNIX)
    if (WIN32)
      if (f MATCHES ".*/host/platform/linux/[^/]*$")
          continue()
      endif()
      if (f MATCHES ".*/platform/linux/[^/]*$")
          continue()
      endif()
    endif (WIN32)
    if (MSVC)
      if (f MATCHES ".*/platform/mingw/[^/]*$")
          continue()
      endif()
    endif (MSVC)
    list(APPEND HDR_FILES ${f})
endforeach()

foreach(f ${CPP_ALL_FILES})
    if (f MATCHES ".*/main.cpp$")
        continue()
    endif()
    if (f MATCHES ".*/main_.*.cpp$")
        continue()
    endif()
    if (f MATCHES ".*/plugins/plugin-window.cpp$")
        continue()
    endif()
    if (f MATCHES ".*/vstsdk-plugin-2.4/vstplugmain.cpp$")
        continue()
    endif()
    if (f MATCHES ".*/tests/[^/]*$")
        continue()
    endif()
    if (UNIX)
	    if (f MATCHES ".*/host/platform/win/[^/]*$")
	        continue()
	    endif()
	    if (f MATCHES ".*/platform/win/[^/]*$")
	        continue()
	    endif()
	    if (f MATCHES ".*/platform/mingw/[^/]*$")
	        continue()
	    endif()
	    if (f MATCHES ".*/external_asio.cpp$")
	        continue()
	    endif()
	  endif (UNIX)
    if (WIN32)
	    if (f MATCHES ".*/host/platform/linux/[^/]*$")
	        continue()
	    endif()
	    if (f MATCHES ".*/platform/linux/[^/]*$")
	        continue()
	    endif()
	  endif (WIN32)
    if (MSVC)
	    if (f MATCHES ".*/platform/mingw/[^/]*$")
	        continue()
	    endif()
	  endif (MSVC)
    list(APPEND CPP_SRC_FILES ${f})
endforeach()

if (BUILD_TESTS)
    list(APPEND CPP_SRC_FILES "src/tests/test_environment.cpp")
endif()



if (NOT MSVC)
    # These files are especially important to have optimized
    # In debug builds their slow performance make the program unusable.
    # But I also want to disable mixed optimization levels if possible.
    # It might be an ODR/ABI violation, and I have seen linkers not producing correct results
    set(SRC_FILES_SUBDIR_FILES 
        src/file/fileloader.cpp
        src/file/projectfile.cpp
        src/file/themefile.cpp
        src/file/appsettingsfile.cpp
    )
    # TODO: what does -O without a number/s produce?
    # TODO: don't lower -O level for release builds when reenabling -On files in debug builds
    # set_source_files_properties(${SRC_FILES_SUBDIR_FILES} PROPERTIES COMPILE_OPTIONS "-g;-O")
    set(SRC_FILES_OPTIMIZE 
        src/gl/gl_path.cpp
        src/wave/wav_tesselate.cpp
        src/wave/clip.cpp
        src/host/vsthost.cpp
        src/host/track_graph.cpp
    )
    # set_source_files_properties(${SRC_FILES_OPTIMIZE} PROPERTIES COMPILE_OPTIONS "-g;-O")

    # The SEH assembly will not work correctly on clang mingw if any level of optimization is applied
    set(SRC_FILES_NO_OPTIMIZE 
        src/host/plugin/vst_plugin_dispatch.cpp
    )
    set_source_files_properties(${SRC_FILES_NO_OPTIMIZE} PROPERTIES COMPILE_OPTIONS "-O0")
endif()

set(CPP_SRC_FILES ${CPP_SRC_FILES} ${MM_SRC_FILES})
set(ALL_SRC_FILES ${CPP_SRC_FILES})
list(APPEND ALL_SRC_FILES ${C_SRC_FILES})
list(APPEND ALL_SRC_FILES ${HDR_FILES})

set(GENERATED_SRC_FILES "${CMAKE_BINARY_DIR}/buildinfo.cpp")
list(APPEND CPP_SRC_FILES ${GENERATED_SRC_FILES})
source_group(TREE ${DAW_SRC_PATH} PREFIX "src" FILES ${ALL_SRC_FILES})
source_group("generated" FILES ${GENERATED_SRC_FILES})

list(APPEND RESOURCE_FILES "")
if (MSVC)
    list(APPEND RESOURCE_FILES "src/icon_resource.h" "src/icon.rc")
endif()

#build a fat library only if BUILD_DAW_LIB is ON
#if BUILD_DAW_LIB is OFF we link objects directly to output executable (faster)
if (BUILD_DAW_LIB) 
    set(LINK_TARGET "dawlib")
else()
    set(LINK_TARGET ${BUILD_NAME})
endif()

include("includes.cmake")
include("libs.cmake")

# instead of an installation step we only copy the exectuable to APP_WORKING_DIR
set(APP_WORKING_DIR "../../run/" CACHE PATH "output directory")

message(STATUS "DAW_DEPS_PATH ${DAW_DEPS_PATH}")
message(STATUS "DEPS_BUILD_FOLDER ${DEPS_BUILD_FOLDER}")
message(STATUS "CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")

message(STATUS "BUILD_DAW_LIB ${BUILD_DAW_LIB}")
message(STATUS "BUILD_NAME ${BUILD_NAME}")
message(STATUS "LINK_TARGET ${LINK_TARGET}")
message(STATUS "APP_WORKING_DIR ${APP_WORKING_DIR}")

message(STATUS "BUILD_HOST ${BUILD_HOST}")
message(STATUS "BUILD_TESTS ${BUILD_TESTS} ")
message(STATUS "BUILD_SCANNER ${BUILD_SCANNER} ")
message(STATUS "BUILD_CMDLINE_HOST ${BUILD_CMDLINE_HOST}")
message(STATUS "BUILD_GUI_TEST ${BUILD_GUI_TEST}")

if (BUILD_DAW_LIB) 
    add_library("dawlib" STATIC ${CPP_SRC_FILES} ${C_SRC_FILES} ${HDR_FILES})
    SET_APP_BUILD("dawlib")
    set_property(TARGET "dawlib" PROPERTY DBG_POSTFIX d)
endif()

if (BUILD_DAW_LIB) 
    add_executable(${BUILD_NAME} "src/host/main_daw.cpp" ${RESOURCE_FILES})
    target_link_libraries(${BUILD_NAME} PUBLIC "dawlib")
else()
    add_executable(${BUILD_NAME} "src/host/main_daw.cpp" ${RESOURCE_FILES} ${CPP_SRC_FILES} ${C_SRC_FILES} ${HDR_FILES})
endif()

# define main exectuable
SET_APP_BUILD(${BUILD_NAME})
ADD_POST_BUILD_COMMANDS(${BUILD_NAME})



target_link_libraries(${LINK_TARGET} PUBLIC optimized ${GLFW_LIB_RELEASE} debug ${GLFW_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${SQLITECPP_LIB_RELEASE} debug ${SQLITECPP_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${SOXR_LIB_RELEASE} debug ${SOXR_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${PORTAUDIO_LIB_RELEASE} debug ${PORTAUDIO_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${PORTMIDI_LIB_RELEASE} debug ${PORTMIDI_LIB_DEBUG})

if (LINUX)
    target_link_libraries(${LINK_TARGET} PRIVATE ${X11_X11_LIB} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} ${ALSA_LIBRARY} ${GTK3_LIBRARIES} GL rt m)
endif(LINUX)
if (APPLE)
    target_link_libraries(${LINK_TARGET} PRIVATE 
        ${CMAKE_THREAD_LIBS_INIT} 
        ${CMAKE_DL_LIBS} 
        ${APPLE_COREMIDI_LIBRARY}
        ${APPLE_COREVIDEO_LIBRARY}
        ${APPLE_OPENGL_LIBRARY}
        ${APPLE_IOKIT_LIBRARY}
        ${APPLE_COCOA_LIBRARY}
        ${APPLE_CARBON_LIBRARY}
        ${APPLE_FOUNDATION_LIBRARY}
        ${APPLE_AUDIOUNIT_LIBRARY}
        ${APPLE_AUDIOTOOLBOX_LIBRARY}
        ${APPLE_COREAUDIO_LIBRARY}
        m)
endif (APPLE)
if (WIN32)
    target_link_libraries(${LINK_TARGET} PRIVATE dbghelp winmm opengl32 kernel32 gdi32 ole32 uuid comdlg32 setupapi mswsock ws2_32)
endif (WIN32)




if (BUILD_SCANNER) 
    add_executable("vstscanner" "src/host/main_scanner.cpp")
    SET_APP_BUILD("vstscanner")
    target_link_libraries("vstscanner" PUBLIC "dawlib")
    ADD_POST_BUILD_COMMANDS("vstscanner")
endif()

if (BUILD_CMDLINE_HOST) 
    add_executable("host" "src/host/main_cmdline.cpp")
    SET_APP_BUILD("host")
    target_link_libraries("host" PUBLIC "dawlib")
    ADD_POST_BUILD_COMMANDS("host")
endif()

if (BUILD_GUI_TEST) 
    add_executable("guitest" "src/host/main_test.cpp")
    SET_APP_BUILD("guitest")
    target_link_libraries("guitest" PUBLIC "dawlib")
    ADD_POST_BUILD_COMMANDS("guitest")
endif()


if (BUILD_RGB_MASTER) 
    add_executable("rgbmaster" "src/rgb/main_rgb_master.cpp")
    SET_APP_BUILD("rgbmaster")
    target_link_libraries("rgbmaster" PUBLIC "dawlib")
    ADD_POST_BUILD_COMMANDS("rgbmaster")
endif()

# march=native optimzation. Turn off for dist build
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if (COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

GENERATE_BUILDINFO_CPP()

if (BUILD_TESTS)
  ENABLE_TESTING()
  ADD_SUBDIRECTORY( src/tests )
endif()
