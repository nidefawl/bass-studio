cmake_minimum_required(VERSION 3.20)
set(PROJECT_NAME "DAW" CACHE STRING "Project name")
project(${PROJECT_NAME} C CXX)
include("cmake/setup.cmake")
# if (NOT DEFINED PROJECT_EXTERNAL_INCLUDE)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${PROJECT_SRC_PATH}/../cmake/msvc-init-override.cmake")
# endif()
include("cmake/libs.cmake")
include("cmake/warnings.cmake")

option(PROJECT_BUILD_DAW "Build daw" ON)
option(PROJECT_BUILD_SCANNER "Build plugin scanner" ON)
option(PROJECT_BUILD_CLI_HOST "Build command line host" ON)
option(PROJECT_BUILD_TESTS "Build tests" ON)
option(PROJECT_CFG_USE_OPENGL3 "Use OpenGL 3.x instead of OpenGL 2" ON)
option(PROJECT_FORCE_LIB "Force building a lib" ON)


if (PROJECT_BUILD_TESTS OR PROJECT_BUILD_SCANNER OR PROJECT_BUILD_CLI_HOST)
    set(PROJECT_REQUIRE_LIB ON)
else ()
    set(PROJECT_REQUIRE_LIB OFF)
endif ()

if (PROJECT_FORCE_LIB) 
    set(PROJECT_REQUIRE_LIB ON)
endif ()



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

foreach (f ${HDR_ALL_FILES})
    if (f MATCHES ".*src/tests/.*$")
        continue()
    endif ()
    if (f MATCHES ".*src/icon_resource.h$")
        continue()
    endif ()
    if (UNIX)
        if (f MATCHES ".*/platform/win/.*$")
            continue()
        endif ()
        if (f MATCHES ".*/platform/mingw/.*$")
            continue()
        endif ()
        if (f MATCHES ".*/external_asio\.cpp$")
            continue()
        endif ()
    endif (UNIX)
    if (WIN32)
        if (f MATCHES ".*/platform/linux/.*$")
            continue()
        endif ()
    endif (WIN32)
    if (MSVC)
        if (f MATCHES ".*/platform/mingw/.*$")
            continue()
        endif ()
    endif (MSVC)
    list(APPEND HDR_FILES ${f})
endforeach ()

foreach (f ${CPP_ALL_FILES})
    if (f MATCHES ".*/main\.cpp$")
        continue()
    endif ()
    if (f MATCHES ".*/main_.*\.cpp$")
        continue()
    endif ()
    if (f MATCHES ".*/plugins/plugin-window\.cpp$")
        continue()
    endif ()
    if (f MATCHES ".*/vstsdk-plugin-2.4/vstplugmain\.cpp$")
        continue()
    endif ()
    if (f MATCHES ".*src/tests/.*$")
        continue()
    endif ()
    if (UNIX)
        if (f MATCHES ".*/platform/win/.*$")
            continue()
        endif ()
        if (f MATCHES ".*/platform/mingw/.*$")
            continue()
        endif ()
        if (f MATCHES ".*/external_asio\.cpp$")
            continue()
        endif ()
    endif (UNIX)
    if (WIN32)
        if (f MATCHES ".*/platform/linux/.*$")
            continue()
        endif ()
    endif (WIN32)
    if (MSVC)
        if (f MATCHES ".*/platform/mingw/.*$")
            continue()
        endif ()
    endif (MSVC)
    list(APPEND CPP_SRC_FILES ${f})
endforeach ()
list(APPEND CPP_SRC_FILES "src/tests/common/test_environment.cpp")

foreach (f ${CPP_ALL_FILES})
if (f MATCHES ".*host/plugin/.*"
    OR f MATCHES ".*window/window_dbg.*" 
    OR f MATCHES ".*plugins/.*"
    OR f MATCHES ".*main/.*")
    set_source_files_properties(${f} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
endif ()

endforeach ()


if (NOT MSVC)
    # These files are especially important to have optimized, even in debug builds
    # Right now this is disabled because of the ODR rule on inline functions (in library headers)
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
            src/wave/waveform_generate.cpp
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
    set_source_files_properties(${SRC_FILES_NO_OPTIMIZE} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

endif ()

set(PROJECT_DEPS_SRC_FILES "${PROJECT_DEPS_PATH}/glad/gl-3.2-core/src/glad.c")
set(GENERATED_SRC_FILES "${CMAKE_BINARY_DIR}/buildinfo.cpp")

list(APPEND RESOURCE_FILES "")
if (MSVC)
    list(APPEND RESOURCE_FILES "src/icon_resource.h" "src/icon.rc")
endif ()

source_group(TREE ${PROJECT_SRC_PATH} PREFIX "src" FILES 
                    ${CPP_SRC_FILES}
                    ${HDR_FILES}
                    ${C_SRC_FILES}
                    ${MM_SRC_FILES})
source_group("generated" FILES ${GENERATED_SRC_FILES})
source_group("deps" FILES ${PROJECT_DEPS_SRC_FILES})
source_group("res" FILES ${RESOURCE_FILES})


message(STATUS "PROJECT_DEPS_PATH ${PROJECT_DEPS_PATH}")
message(STATUS "CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")
message(STATUS "PROJECT_WORKING_DIR ${PROJECT_WORKING_DIR}")
message(STATUS "PROJECT_REQUIRE_LIB ${PROJECT_REQUIRE_LIB}")
message(STATUS "PROJECT_BUILD_DAW ${PROJECT_BUILD_DAW}")
message(STATUS "PROJECT_BUILD_TESTS ${PROJECT_BUILD_TESTS} ")
message(STATUS "PROJECT_BUILD_SCANNER ${PROJECT_BUILD_SCANNER} ")
message(STATUS "PROJECT_BUILD_CLI_HOST ${PROJECT_BUILD_CLI_HOST}")
message(STATUS "BUILD WARNINGS ${BUILD_WARNING_FLAGS}")

# build a fat library only if PROJECT_REQUIRE_LIB is ON
# if PROJECT_REQUIRE_LIB is OFF we link objects directly to output executable (faster)
if (PROJECT_REQUIRE_LIB)
    set(LINK_TARGET "dawlib")
    add_library(${LINK_TARGET} STATIC 
                            ${CPP_SRC_FILES} 
                            ${C_SRC_FILES} 
                            ${MM_SRC_FILES} 
                            ${GENERATED_SRC_FILES} 
                            ${PROJECT_DEPS_SRC_FILES} 
                            ${HDR_FILES})
    set_property(TARGET ${LINK_TARGET} PROPERTY OUTPUT_NAME "daw-lib")
    set_property(TARGET ${LINK_TARGET} PROPERTY DEBUG_POSTFIX d)
else ()
    set(LINK_TARGET ${PROJECT_NAME})
    # define main exectuable
    add_executable(${PROJECT_NAME} "src/host/main_daw.cpp"
                            ${RESOURCE_FILES} 
                            ${CPP_SRC_FILES} 
                            ${C_SRC_FILES} 
                            ${MM_SRC_FILES} 
                            ${GENERATED_SRC_FILES} 
                            ${PROJECT_DEPS_SRC_FILES} 
                            ${HDR_FILES})
    SET_APP_BUILD(${PROJECT_NAME})
    ADD_POST_BUILD_COMMANDS(${PROJECT_NAME})
endif ()


target_compile_definitions(${LINK_TARGET} PUBLIC GLFW_INCLUDE_NONE=1 USE_WIN32_EXC_HOOKS BUILD_VSTHOST=1 BUILD_EXTERNAL_PLUGIN=0)
#target_compile_definitions(${LINK_TARGET} PUBLIC PA_ENABLE_DEBUG_OUTPUT=1)
target_compile_definitions(${LINK_TARGET} PUBLIC VST_FORCE_DEPRECATED=0 GLM_FORCE_CXX14=1)

if (WIN32)
    target_compile_definitions(${LINK_TARGET} PUBLIC PA_USE_ASIO=1 PA_USE_DS=1 PA_USE_WASAPI=1 PA_USE_WDMKS=1)
endif (WIN32)

if (UNIX)
    target_compile_definitions(${LINK_TARGET} PUBLIC PA_USE_ALSA=1 __cdecl="")
endif (UNIX)

if (PROJECT_CFG_USE_OPENGL3)
    target_compile_definitions(${LINK_TARGET} PUBLIC NANOVG_GL3=1)
else ()
    target_compile_definitions(${LINK_TARGET} PUBLIC NANOVG_GL2=1)
endif ()

if (PROJECT_BUILD_TESTS)
    target_compile_definitions(${LINK_TARGET} PUBLIC PROJECT_BUILD_TESTS)
endif ()



target_compile_options(${LINK_TARGET} PUBLIC ${BUILD_WARNING_FLAGS})

# march=native optimization. Turn off for dist build
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if (COMPILER_SUPPORTS_MARCH_NATIVE)
    target_compile_options(${LINK_TARGET} PUBLIC -march=native)
endif ()



if (PROJECT_REQUIRE_LIB)

    MACRO(DEFINE_EXECUTABLE target_name target_sources)
        source_group("src" FILES ${target_sources})
        add_executable(${target_name} ${target_sources})
        target_link_libraries(${target_name} PRIVATE "dawlib")
        target_compile_options(${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
        SET_APP_BUILD(${target_name})
        ADD_POST_BUILD_COMMANDS(${target_name})
    ENDMACRO()

    # define main executable
    if (PROJECT_BUILD_DAW)
        DEFINE_EXECUTABLE(${PROJECT_NAME} "src/host/main_daw.cpp;${RESOURCE_FILES}")
    endif ()

    if (PROJECT_BUILD_SCANNER)
        DEFINE_EXECUTABLE("vstscanner" "src/host/main_scanner.cpp")
    endif ()

    if (PROJECT_BUILD_CLI_HOST)
        DEFINE_EXECUTABLE("host" "src/host/main_cmdline.cpp")
    endif ()
endif ()

if (WIN32)
    target_precompile_headers(${LINK_TARGET} PUBLIC <Windows.h>)
endif (WIN32)

target_precompile_headers(${LINK_TARGET} PUBLIC 
    <Windows.h>
    "$<$<COMPILE_LANGUAGE:CXX>:<cstddef$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<functional$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<array$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
    "$<$<COMPILE_LANGUAGE:CXX>:include/math/vec.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:include/basectrl.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:host/mainctrl.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:include/fileio.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:include/renderresources.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:gui/gui.h>"
    "$<$<COMPILE_LANGUAGE:CXX>:${PROJECT_DEPS_PATH}/glm/glm/glm.hpp>"
)

target_link_libraries(${LINK_TARGET} PUBLIC optimized ${GLFW_LIB_RELEASE} debug ${GLFW_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${SQLITECPP_LIB_RELEASE} debug ${SQLITECPP_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${SQLITE3_LIB_RELEASE} debug ${SQLITE3_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${SOXR_LIB_RELEASE} debug ${SOXR_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${KISSFFT_LIB_RELEASE} debug ${KISSFFT_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${PORTAUDIO_LIB_RELEASE} debug ${PORTAUDIO_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${PORTMIDI_LIB_RELEASE} debug ${PORTMIDI_LIB_DEBUG})

if (LINUX)
    target_link_libraries(${LINK_TARGET} PUBLIC ${X11_X11_LIB} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} ${ALSA_LIBRARY} ${GTK3_LIBRARIES} GL rt m)
endif (LINUX)
if (APPLE)
    target_link_libraries(${LINK_TARGET} PUBLIC
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
    set_target_properties(${LINK_TARGET} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    target_link_libraries(${LINK_TARGET} PUBLIC dbghelp winmm opengl32 kernel32 gdi32 ole32 uuid comdlg32 setupapi mswsock ws2_32)
endif (WIN32)

if (WIN32 AND PROJECT_CFG_USE_STACK_PROTECTOR)
    target_link_libraries(${LINK_TARGET} PUBLIC ssp)
endif ()

SET_TARGET_PROJECT_INCLUDE_DIRS(${LINK_TARGET})

GENERATE_BUILDINFO_CPP(${LINK_TARGET} src/app/buildinfo.cpp.in)

if (PROJECT_BUILD_TESTS)
    ENABLE_TESTING()
    ADD_SUBDIRECTORY(src/tests)
endif ()

set(CMAKE_VERBOSE_MAKEFILE ON)