cmake_minimum_required(VERSION 3.3)
# Don't use -rdynamic
if (POLICY CMP0065)
    cmake_policy(SET CMP0065 NEW)
endif ()
set(CMAKE_USER_MAKE_RULES_OVERRIDE "cmake/msvc-init-override.cmake")

message(STATUS "CMake version: ${CMAKE_VERSION}")
set(DAW_SRC_PATH "${CMAKE_SOURCE_DIR}/src")
set(BUILD_NAME "DAW") #TODO rename BUILD_NAME variable
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${BUILD_NAME})

project(${BUILD_NAME} C CXX)
option(BUILD_HOST "Build daw" ON)
option(BUILD_SCANNER "Build plugin scanner" ON)
option(BUILD_CMDLINE_HOST "Build command line host" ON)
option(BUILD_TESTS "Build tests" ON)
option(USE_OPENGL3 "Use OpenGL 3.x instead of OpenGL 2" ON)
# instead of an installation step we only copy the exectuable to APP_WORKING_DIR
set(APP_WORKING_DIR "../../run/" CACHE PATH "working directory (run)")

if (BUILD_TESTS OR BUILD_SCANNER OR BUILD_CMDLINE_HOST)
    set(BUILD_DAW_LIB ON)
else ()
    set(BUILD_DAW_LIB OFF)
endif ()

#set(BUILD_DAW_LIB ON) #override

if (NOT MSVC)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CMAKE_COMMAND} -E time")
endif ()
include("cmake/setup.cmake")
include("cmake/libs.cmake")

add_compile_definitions(USE_WIN32_EXC_HOOKS BUILD_VSTHOST=1 BUILD_EXTERNAL_PLUGIN=0)
#add_compile_definitions(PA_ENABLE_DEBUG_OUTPUT=1)
add_compile_definitions(VST_FORCE_DEPRECATED=0 USE_GLAD_GL_HEADERS GLM_FORCE_CXX14=1)
if (WIN32)
    add_compile_definitions(PA_USE_ASIO=1 PA_USE_DS=1 PA_USE_WASAPI=1 PA_USE_WDMKS=1)
endif (WIN32)
if (UNIX)
    add_compile_definitions(PA_USE_ALSA=1 __cdecl="")
endif (UNIX)
if (USE_OPENGL3)
    add_compile_definitions(NANOVG_GL3=1)
else ()
    add_compile_definitions(NANOVG_GL2=1)
endif ()
if (BUILD_TESTS)
    add_compile_definitions(BUILD_TESTS)
endif ()

# set warn flags
if (MSVC)

    set(MSVC_WARNINGS_DEBUG 
        /W4
        # Disabled warnings
        /wd4267 # size_t to int
        /wd4244 # float to int
        /wd4100 # unreferenced formal parameter
        /wd4458 # declaration of x hides class member
        # Enabled warnings not covered by /W4
        /w14263 # 'function': member function does not override any base class virtual member function
        /w14265 # 'classname': class has virtual functions, but destructor is not virtual instances of this class may not
                # be destructed correctly
        /w14287 # 'operator': unsigned/negative constant mismatch
        /we4289 # nonstandard extension used: 'variable': loop control variable declared in the for-loop is used outside
                # the for-loop scope
        /w14296 # 'operator': expression is always 'boolean_value'
        /w14311 # 'variable': pointer truncation from 'type1' to 'type2'
        /w14545 # expression before comma evaluates to a function which is missing an argument list
        /w14546 # function call before comma missing argument list
        /w14547 # 'operator': operator before comma has no effect; expected operator with side-effect
        /w14549 # 'operator': operator before comma has no effect; did you intend 'operator'?
        /w14555 # expression has no effect; expected expression with side- effect
        /w14619 # pragma warning: there is no warning number 'number'
        /w14640 # Enable warning on thread un-safe static member initialization
        /w14826 # Conversion from 'type1' to 'type_2' is sign-extended. This may cause unexpected runtime behavior.
        /w14905 # wide string literal cast to 'LPSTR'
        /w14906 # string literal cast to 'LPWSTR'
        /w14928 # illegal copy-initialization; more than one user-defined conversion has been implicitly applied
        /permissive- # standards conformance mode for MSVC compiler.
    )
    set(MSVC_WARNINGS_RELEASE /W3 /wd4267 /wd4244 /wd4100 /wd4458 /wd4305)
    set(BUILD_WARNING_FLAGS $<$<CONFIG:Debug>:${MSVC_WARNINGS_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${MSVC_WARNINGS_RELEASE}>)
else ()
    set(BUILD_WARNING_FLAGS -Wall -Wno-inconsistent-missing-override -Wno-unused-parameter)
endif ()

# march=native optimzation. Turn off for dist build
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if (COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
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
endif ()

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
endif ()

message(STATUS "DAW_DEPS_PATH ${DAW_DEPS_PATH}")
message(STATUS "CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}")

message(STATUS "APP_WORKING_DIR ${APP_WORKING_DIR}")

message(STATUS "BUILD_DAW_LIB ${BUILD_DAW_LIB}")
message(STATUS "BUILD_HOST ${BUILD_HOST}")
message(STATUS "BUILD_TESTS ${BUILD_TESTS} ")
message(STATUS "BUILD_SCANNER ${BUILD_SCANNER} ")
message(STATUS "BUILD_CMDLINE_HOST ${BUILD_CMDLINE_HOST}")
message(STATUS "BUILD WARNINGS ${BUILD_WARNING_FLAGS}")

# build a fat library only if BUILD_DAW_LIB is ON
# if BUILD_DAW_LIB is OFF we link objects directly to output executable (faster)
if (BUILD_DAW_LIB)
    set(LINK_TARGET "dawlib")
    add_library(${LINK_TARGET} STATIC ${CPP_SRC_FILES} ${C_SRC_FILES} ${HDR_FILES})
    target_compile_options(${LINK_TARGET} PRIVATE ${BUILD_WARNING_FLAGS})
    set_property(TARGET ${LINK_TARGET} PROPERTY DBG_POSTFIX d)
else ()
    set(LINK_TARGET ${BUILD_NAME})
    # define main exectuable
    add_executable(${BUILD_NAME} "src/host/main_daw.cpp" ${RESOURCE_FILES} ${CPP_SRC_FILES} ${C_SRC_FILES} ${HDR_FILES})
    SET_APP_BUILD(${BUILD_NAME})
    ADD_POST_BUILD_COMMANDS(${BUILD_NAME})
    target_compile_options(${BUILD_NAME} PRIVATE ${BUILD_WARNING_FLAGS})
endif ()

MACRO(DEFINE_EXECUTABLE target_name target_sources)
    add_executable(${target_name} ${target_sources})
    SET_APP_BUILD(${target_name})
    target_link_libraries(${target_name} PUBLIC "dawlib")
    ADD_POST_BUILD_COMMANDS(${target_name})
    target_compile_options(${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
ENDMACRO()

if (BUILD_DAW_LIB)
    # define main exectuable
    list(APPEND DAW_MAIN_SRC "src/host/main_daw.cpp")
    list(APPEND DAW_MAIN_SRC ${RESOURCE_FILES})
    DEFINE_EXECUTABLE(${BUILD_NAME} ${DAW_MAIN_SRC})

    if (BUILD_SCANNER)
        DEFINE_EXECUTABLE("vstscanner" "src/host/main_scanner.cpp")
    endif ()

    if (BUILD_CMDLINE_HOST)
        DEFINE_EXECUTABLE("host" "src/host/main_cmdline.cpp")
    endif ()
endif ()

target_link_libraries(${LINK_TARGET} PUBLIC optimized ${GLFW_LIB_RELEASE} debug ${GLFW_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${SQLITECPP_LIB_RELEASE} debug ${SQLITECPP_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${SOXR_LIB_RELEASE} debug ${SOXR_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${PORTAUDIO_LIB_RELEASE} debug ${PORTAUDIO_LIB_DEBUG})
target_link_libraries(${LINK_TARGET} PUBLIC optimized ${PORTMIDI_LIB_RELEASE} debug ${PORTMIDI_LIB_DEBUG})

if (LINUX)
    target_link_libraries(${LINK_TARGET} PRIVATE ${X11_X11_LIB} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} ${ALSA_LIBRARY} ${GTK3_LIBRARIES} GL rt m)
endif (LINUX)
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


GENERATE_BUILDINFO_CPP()

if (BUILD_TESTS)
    ENABLE_TESTING()
    ADD_SUBDIRECTORY(src/tests)
endif ()
