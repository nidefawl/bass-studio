cmake_minimum_required(VERSION 3.3)
message (STATUS "CMake version: ${CMAKE_VERSION}")

set(DAW_SRC_PATH "${CMAKE_SOURCE_DIR}/src")
set(BUILD_NAME "DAW")

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${BUILD_NAME})
# set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${BUILD_NAME})
project(${BUILD_NAME} C CXX)
option(BUILD_HOST "Build daw" ON)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_SCANNER "Build plugin scanner" OFF)
option(BUILD_CMDLINE_HOST "Build command line host" OFF)
option(BUILD_GUI_TEST "Build test app" OFF)

if (BUILD_TESTS OR BUILD_SCANNER OR BUILD_CMDLINE_HOST OR BUILD_GUI_TEST)
set(BUILD_DAW_LIB ON)
else()
set(BUILD_DAW_LIB OFF)
endif ()

#set(BUILD_DAW_LIB ON) #override

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")
include("setup.cmake")
include("cotire.cmake")
add_definitions("-DUSE_WIN32_EXC_HOOKS")
add_definitions("-DBUILD_VSTHOST=1")
add_definitions("-DBUILD_EXTERNAL_PLUGIN=0")
add_definitions("-DNANOVG_GL2=1")
# add_definitions("-DNANOVG_GL3=1")


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
        message("STATUS SKIPPING SRC ${f}")
        continue()
    endif()
    if (f MATCHES ".*/main_.*.cpp$")
        message("STATUS SKIPPING SRC ${f}")
        continue()
    endif()
    if (f MATCHES ".*/plugins/plugin-window.cpp$")
         message("STATUS SKIPPING SRC ${f}")
        continue()
    endif()
    if (f MATCHES ".*/vstsdk-plugin-2.4/vstplugmain.cpp$")
         message("STATUS SKIPPING SRC ${f}")
        continue()
    endif()
    if (f MATCHES ".*/tests/[^/]*$")
        message("STATUS SKIPPING SRC ${f}")
        continue()
    endif()
    if (f MATCHES ".*/file/[^/]*$")
        message("STATUS SKIPPING SRC ${f}")
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
set(CPP_SRC_FILES ${SRC_FILES_SUBDIR_FILES} ${CPP_SRC_FILES})

list(APPEND CPP_SRC_FILES ${CMAKE_BINARY_DIR}/buildinfo.cpp)

set(SRC_FILES_SUBDIR_FILES 
    src/file/fileloader.cpp
    src/file/projectfile.cpp
    src/file/themefile.cpp
    src/file/appsettingsfile.cpp
)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set_source_files_properties(${SRC_FILES_SUBDIR_FILES} PROPERTIES COMPILE_OPTIONS "-g;-O")
    set(SRC_FILES_OPTIMIZE 
        src/gl/gl_path.cpp
        src/wave/wav_tesselate.cpp
        src/wave/clip.cpp
        src/host/vsthost.cpp
        src/host/track_graph.cpp
    )
    set_source_files_properties(${SRC_FILES_OPTIMIZE} PROPERTIES COMPILE_OPTIONS "-g;-O")
endif()

set(CPP_SRC_FILES ${SRC_FILES_SUBDIR_FILES} ${CPP_SRC_FILES} ${MM_SRC_FILES})
source_group("Sources" FILES ${CPP_SRC_FILES})
source_group("Headers" FILES ${HDR_FILES})

#build a fat library only when tests are enabled
#if tests are disabled we link objects directly to output executable (faster)
if (BUILD_DAW_LIB) 
set(LINK_TARGET "dawlib")
else()
set(LINK_TARGET ${BUILD_NAME})
endif()

message(STATUS "${BUILD_HOST} Build daw")
message(STATUS "${BUILD_TESTS} Build tests")
message(STATUS "${BUILD_SCANNER} Build plugin scanner")
message(STATUS "${BUILD_CMDLINE_HOST} Build command line host")
message(STATUS "${BUILD_GUI_TEST} Build test app")
message(STATUS "BUILD_NAME ${BUILD_NAME}")
message(STATUS "DAW_DEPS_PATH ${DAW_DEPS_PATH}")
message(STATUS "DEPS_BUILD_FOLDER ${DEPS_BUILD_FOLDER}")
message(STATUS "BUILD_NAME ${BUILD_NAME}")
message(STATUS "LINK_TARGET ${LINK_TARGET}")
message(STATUS "CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")

# target_link_libraries(${LINK_TARGET} PUBLIC ${GLFW_LIB} ${SQLITECPP_LIB} ${SOXR_LIB} ${PORTAUDIO_LIB})

include("includes.cmake")
include("libs.cmake")


set(DAW_WORKING_DIR "../../run/" CACHE PATH "output directory")
message(STATUS "DAW_WORKING_DIR ${DAW_WORKING_DIR}")

FUNCTION(SET_APP_BUILD appname)
    set(tmp1 "${appname}-${BUILD_BINARY_SUFFIX}")
    set_target_properties(${appname} PROPERTIES OUTPUT_NAME ${tmp1})
    set_target_properties(${appname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${DAW_WORKING_DIR}$<$<CONFIG:Debug>:>)
    set_target_properties(${appname} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${DAW_WORKING_DIR})
    set_target_properties(${appname} PROPERTIES OUTPUT_NAME_DEBUG "${appname}-${CMAKE_CXX_COMPILER_ID}-debug${OUTPUT_BINARY_SUFFIX}" )
    set_target_properties(${appname} PROPERTIES OUTPUT_NAME_RELEASE "${appname}-${CMAKE_CXX_COMPILER_ID}-release${OUTPUT_BINARY_SUFFIX}" )
ENDFUNCTION(SET_APP_BUILD)

if (BUILD_DAW_LIB) 
    add_library("dawlib" STATIC ${CPP_SRC_FILES} ${C_SRC_FILES} ${HDR_FILES})
    SET_APP_BUILD("dawlib")
    set_property(TARGET "dawlib" PROPERTY DBG_POSTFIX d)
endif()

if (BUILD_DAW_LIB) 
add_executable(${BUILD_NAME} "src/host/main_daw.cpp")
target_link_libraries(${BUILD_NAME} PUBLIC "dawlib")
else()
add_executable(${BUILD_NAME} "src/host/main_daw.cpp" ${CPP_SRC_FILES} ${C_SRC_FILES} ${HDR_FILES})
endif()
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
endif(APPLE)
if (WIN32)
target_link_libraries(${LINK_TARGET} PRIVATE dbghelp winmm opengl32 kernel32 gdi32 ole32 uuid comdlg32 setupapi mswsock ws2_32)
endif(WIN32)




if (BUILD_SCANNER) 
project("vstscanner" C CXX)
add_executable("vstscanner" "src/host/main_scanner.cpp")
SET_APP_BUILD("vstscanner")
target_link_libraries("vstscanner" PUBLIC "dawlib")
ADD_POST_BUILD_COMMANDS("vstscanner")
endif()

if (BUILD_CMDLINE_HOST) 
project("host" C CXX)
add_executable("host" "src/host/main_cmdline.cpp")
SET_APP_BUILD("host")
target_link_libraries("host" PUBLIC "dawlib")
ADD_POST_BUILD_COMMANDS("host")
endif()

if (BUILD_GUI_TEST) 
project("guitest" C CXX)
add_executable("guitest" "src/host/main_test.cpp")
SET_APP_BUILD("guitest")
target_link_libraries("guitest" PUBLIC "dawlib")
ADD_POST_BUILD_COMMANDS("guitest")
endif()


if (BUILD_RGB_MASTER) 
project("rgbmaster" C CXX)
add_executable("rgbmaster" "src/host/main_rgb_master.cpp")
SET_APP_BUILD("rgbmaster")
target_link_libraries("rgbmaster" PUBLIC "dawlib")
ADD_POST_BUILD_COMMANDS("rgbmaster")
endif()

set(COTIRE_UNITY_SOURCE_EXCLUDE_EXTENSIONS "m;mm;c")
message(STATUS "COTIRE_UNITY_SOURCE_EXCLUDE_EXTENSIONS: ${COTIRE_UNITY_SOURCE_EXCLUDE_EXTENSIONS}")
set_source_files_properties("${DAW_SRC_PATH}/platform/win/DropTarget.cpp" PROPERTIES COTIRE_EXCLUDED TRUE)
set_source_files_properties("${DAW_SRC_PATH}/platform/win/fileio.cpp" PROPERTIES COTIRE_EXCLUDED TRUE)
set_source_files_properties("${DAW_SRC_PATH}/window/window.cpp" PROPERTIES COTIRE_EXCLUDED TRUE)
set_source_files_properties("${DAW_SRC_PATH}/window/debugwindow.cpp" PROPERTIES COTIRE_EXCLUDED TRUE)
# set_target_properties(${BUILD_NAME} PROPERTIES COTIRE_PREFIX_HEADER_IGNORE_PATH "${DAW_SRC_PATH}/nanovg")

# set_source_files_properties("src/render.cpp" PROPERTIES COTIRE_EXCLUDED TRUE)
set(NANOVG_HDR_PATH "${DAW_SRC_PATH}/nanovg/")
#set_target_properties(${BUILD_NAME} PROPERTIES COTIRE_PREFIX_HEADER_IGNORE_PATH "${CMAKE_SOURCE_DIR};${NANOVG_HDR_PATH}")

# cotire(${BUILD_NAME}) #comment to disable cotire

if (BUILD_TESTS)
  ENABLE_TESTING()
  ADD_SUBDIRECTORY( src/tests )
endif()
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

get_property(BUILD_COMPILE_OPTIONS        DIRECTORY  PROPERTY COMPILE_OPTIONS)
get_property(BUILD_COMPILE_DEFS           DIRECTORY  PROPERTY COMPILE_DEFINITIONS)
get_property(BUILD_CXX_STANDARD           TARGET ${BUILD_NAME}  PROPERTY CXX_STANDARD)
string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE_SUFFIX)
separate_arguments(GLOBAL_FLAGS UNIX_COMMAND "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BUILD_TYPE_SUFFIX}}")
list(APPEND BUILD_COMPILE_OPTIONS ${GLOBAL_FLAGS})
list(APPEND BUILD_COMPILE_OPTIONS -std=c++${BUILD_CXX_STANDARD})
string(REPLACE ";" " " BUILD_COMPILE_OPTIONS "${BUILD_COMPILE_OPTIONS}")
string(REPLACE ";" " -D" BUILD_COMPILE_DEFS "-D${BUILD_COMPILE_DEFS}")
CONFIGURE_FILE( src/app/buildinfo.cpp.in ${CMAKE_BINARY_DIR}/buildinfo.cpp ESCAPE_QUOTES)
