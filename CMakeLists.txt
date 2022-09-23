cmake_minimum_required(VERSION 3.21)
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(PROJECT_NAME "DAW" CACHE STRING "Project name")
set(CMAKE_USER_MAKE_RULES_OVERRIDE FixCMakeInitFlags)
project(${PROJECT_NAME} LANGUAGES C CXX VERSION 0.5.1)
include(GetGitRevision)
include(CompilerConfig)
include(BuildConfig)
include(WarningFlags)
include(FindLibraries)

option(PROJECT_BUILD_DAW "Build daw" ON)
option(PROJECT_BUILD_SCANNER "Build plugin scanner" ON)
option(PROJECT_BUILD_CLI_HOST "Build command line host" ON)
option(PROJECT_BUILD_TESTS "Build tests" ON)
option(PROJECT_BUILD_BENCHMARKS "Build benchmarks" ON)
option(PROJECT_BUILD_PLUGINS "Build standalone plugins" ON)
option(PROJECT_CFG_USE_OPENGL3 "Use OpenGL 3.x instead of OpenGL 2" ON)
option(PROJECT_FORCE_LIB "Force building a lib" ON)
option(PROJECT_GENERATE_LINKER_MAP "Generate <build-dir>/<exe>_linker.map" OFF)
set(PROJECT_VST2_OUTPUT_PATH "${ABS_WORKING_DIR}" CACHE STRING "VST2 output path")


if (PROJECT_BUILD_BENCHMARKS
  OR PROJECT_BUILD_TESTS 
  OR PROJECT_BUILD_SCANNER
  OR PROJECT_BUILD_PLUGINS 
  OR PROJECT_BUILD_CLI_HOST)
  set(PROJECT_REQUIRE_LIB ON)
else()
  set(PROJECT_REQUIRE_LIB OFF)
endif()

if (PROJECT_FORCE_LIB) 
  set(PROJECT_REQUIRE_LIB ON)
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

foreach (f ${HDR_ALL_FILES})
  if (f MATCHES "src/icon_resource\.h")
    continue()
  endif()
  if (UNIX AND f MATCHES "/platform/win/")
    continue()
  endif()
  if (WIN32 AND f MATCHES "/platform/linux/")
    continue()
  endif()
  if (MSVC AND f MATCHES "/platform/mingw/")
    continue()
  endif()
  list(APPEND HDR_FILES ${f})
endforeach ()

foreach (f ${CPP_ALL_FILES})
  if (f MATCHES "/host/main_.*\.cpp")
    continue()
  endif()
  if (f MATCHES "/plugins/plugin-window\.cpp")
    continue()
  endif()
  if (f MATCHES "/vstsdk-plugin-2.4/vstplugmain\.cpp")
    continue()
  endif()
  if (UNIX AND f MATCHES "/platform/win/")
    continue()
  endif()
  if (WIN32 AND f MATCHES "/platform/linux/")
    continue()
  endif()
  if (MSVC AND f MATCHES "/platform/mingw/")
    continue()
  endif()
  list(APPEND CPP_SRC_FILES ${f})
endforeach ()

foreach (f ${CPP_ALL_FILES})
  if (f MATCHES ".*host/plugin/.*"
    OR f MATCHES ".*main/.*"
    OR f MATCHES ".*platform/linux/.*")
    set_source_files_properties(${f} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
  endif()
endforeach ()


if (MINGW)
  # The SEH assembly will not work correctly on clang mingw if any level of optimization is applied
  set(SRC_FILES_NO_OPTIMIZE src/host/plugin/vst_plugin_dispatch.cpp)
  set_source_files_properties(${SRC_FILES_NO_OPTIMIZE} PROPERTIES COMPILE_OPTIONS "-fno-lto;-mtune=generic;-fno-function-sections;-fno-data-sections")
  set_source_files_properties(${SRC_FILES_NO_OPTIMIZE} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

if (CLANG)
  set_source_files_properties(src/util/fp_math.cpp PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION ON
    SKIP_PRECOMPILE_HEADERS ON
    COMPILE_OPTIONS "-fno-lto;-mtune=generic;-fno-function-sections;-fno-data-sections;-fno-fast-math;-O2"
  )
  set_source_files_properties(src/plugins/synth/synth-plugin.cpp PROPERTIES
    SKIP_UNITY_BUILD_INCLUSION ON
    SKIP_PRECOMPILE_HEADERS ON
    # COMPILE_OPTIONS "-fno-lto;-Rpass-analysis=loop-vectorize;-Rpass=loop-vectorize;-Rpass-missed=loop-vectorize"
  )
endif()
set(PROJECT_DEP_MUPARSERS_SRC_FILES
  muparser/src/muParser.cpp
  muparser/src/muParserBase.cpp
  muparser/src/muParserBytecode.cpp
  muparser/src/muParserCallback.cpp
  muparser/src/muParserError.cpp
  muparser/src/muParserInt.cpp
  muparser/src/muParserTokenReader.cpp
)
set(PROJECT_DEPS_SRC_FILES
  glad/gl-3.2-core/src/glad.c
  ${PROJECT_DEP_MUPARSERS_SRC_FILES}
)

list(TRANSFORM PROJECT_DEPS_SRC_FILES PREPEND "${PROJECT_DEPS_PATH}/")
if (CMAKE_COMPILER_IS_GNUCC)
  set_source_files_properties("${PROJECT_DEPS_PATH}/glad/gl-3.2-core/src/glad.c" PROPERTIES COMPILE_OPTIONS "-Wno-pedantic")
endif()

list(APPEND RESOURCE_FILES "")
if (WIN32 AND NOT PROJECT_NO_WINDRES)
  list(APPEND RESOURCE_FILES "src/icon_resource.h" "src/icon.rc")
endif()

source_group(TREE   ${PROJECT_SRC_PATH} 
             PREFIX "src" 
             FILES  ${CPP_SRC_FILES}
                    ${HDR_FILES}
                    ${C_SRC_FILES}
                    ${MM_SRC_FILES})

source_group("deps" FILES ${PROJECT_DEPS_SRC_FILES})
source_group("res" FILES ${RESOURCE_FILES})


message(STATUS "CMAKE_CXX_COMPILER_ID ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "PROJECT_WORKING_DIR ${ABS_WORKING_DIR}")
message(STATUS "PROJECT_REQUIRE_LIB ${PROJECT_REQUIRE_LIB}")
message(STATUS "PROJECT_BUILD_DAW ${PROJECT_BUILD_DAW}")
message(STATUS "PROJECT_BUILD_TESTS ${PROJECT_BUILD_TESTS} ")
message(STATUS "PROJECT_BUILD_BENCHMARKS ${PROJECT_BUILD_BENCHMARKS} ")
message(STATUS "PROJECT_BUILD_SCANNER ${PROJECT_BUILD_SCANNER} ")
message(STATUS "PROJECT_BUILD_CLI_HOST ${PROJECT_BUILD_CLI_HOST}")
message(STATUS "PROJECT_BUILD_PLUGINS ${PROJECT_BUILD_PLUGINS}")
message(STATUS "BUILD WARNINGS ${BUILD_WARNING_FLAGS}")

# build a fat library only if PROJECT_REQUIRE_LIB is ON
# if PROJECT_REQUIRE_LIB is OFF we link objects directly to output executable (faster)
if (PROJECT_REQUIRE_LIB)
  set(LINK_TARGET "dawlib")
  add_library(${LINK_TARGET} STATIC 
              ${CPP_SRC_FILES} 
              ${C_SRC_FILES} 
              ${MM_SRC_FILES} 
              ${PROJECT_DEPS_SRC_FILES} 
              ${HDR_FILES})
  set_property(TARGET ${LINK_TARGET} PROPERTY OUTPUT_NAME "daw-lib")
  set_property(TARGET ${LINK_TARGET} PROPERTY DEBUG_POSTFIX d)
else()
  set(LINK_TARGET ${PROJECT_NAME})
  # define main exectuable
  add_executable(${PROJECT_NAME} "src/host/main_daw.cpp"
                ${RESOURCE_FILES} 
                ${CPP_SRC_FILES} 
                ${C_SRC_FILES} 
                ${MM_SRC_FILES} 
                ${PROJECT_DEPS_SRC_FILES} 
                ${HDR_FILES})
  CONFIGURE_TARGET_OUTPUT(${PROJECT_NAME} ${PROJECT_NAME})
endif()


target_compile_definitions(${LINK_TARGET} PUBLIC GLFW_INCLUDE_NONE=1 USE_WIN32_EXC_HOOKS BUILD_VSTHOST=1 BUILD_EXTERNAL_PLUGIN=0)
#target_compile_definitions(${LINK_TARGET} PUBLIC PA_ENABLE_DEBUG_OUTPUT=1)
target_compile_definitions(${LINK_TARGET} PUBLIC GLM_FORCE_SSE42=1 VST_FORCE_DEPRECATED=0 MUPARSER_STATIC=1)

if (PROJECT_CFG_USE_OPENGL3)
  target_compile_definitions(${LINK_TARGET} PUBLIC NANOVG_GL3=1)
else()
  target_compile_definitions(${LINK_TARGET} PUBLIC NANOVG_GL2=1)
endif()

if (PROJECT_BUILD_TESTS)
  target_compile_definitions(${LINK_TARGET} PUBLIC PROJECT_BUILD_TESTS)
endif()

target_compile_options(${LINK_TARGET} PUBLIC ${BUILD_WARNING_FLAGS})

if (PROJECT_REQUIRE_LIB)
  MACRO(DEFINE_EXECUTABLE target_name target_sources)
    source_group("src" FILES ${target_sources})
    add_executable(${target_name} ${target_sources})
    target_link_libraries(${target_name} PRIVATE "dawlib")
    target_compile_options(${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
    CONFIGURE_TARGET_OUTPUT(${target_name} ${target_name})
  ENDMACRO()

  # define main executable
  if (PROJECT_BUILD_DAW)
    DEFINE_EXECUTABLE(${PROJECT_NAME} "src/host/main_daw.cpp;${RESOURCE_FILES}")
  endif()

  if (PROJECT_BUILD_SCANNER)
    DEFINE_EXECUTABLE("vstscanner" "src/host/main_scanner.cpp")
  endif()

  if (PROJECT_BUILD_CLI_HOST)
    DEFINE_EXECUTABLE("host" "src/host/main_cmdline.cpp")
  endif()

  FUNCTION(FN_DL_PLUGIN_BUILD plugBuildName plugBuildOutputName srcfilelist plugindefs)
    add_library(
        ${plugBuildName}
        SHARED 
        ${srcfilelist} 
        "${MAIN_SRC_PATH}/plugins/vst2_dll_exports.def"
      )
    set_source_files_properties(${srcfilelist} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    set_target_properties(${plugBuildName} PROPERTIES PREFIX "")
    source_group(${plugBuildName} FILES ${srcfilelist})
    target_link_libraries(${plugBuildName} PRIVATE "dawlib")
    CONFIGURE_TARGET_OUTPUT(${plugBuildName} ${plugBuildOutputName})

    # target_link_libraries(${plugBuildName} PRIVATE winmm)
    if (CMAKE_COMPILER_IS_GNUCXX)
      set(PLUGIN_LINK_FLAGS "-static-libgcc -static-libstdc++")
      set_target_properties(${plugBuildName} PROPERTIES LINK_FLAGS "${PLUGIN_LINK_FLAGS}")
    endif()
    if (CLANG)
      #set(PLUGIN_LINK_FLAGS "-static")
      set_target_properties(${plugBuildName} PROPERTIES LINK_FLAGS "${PLUGIN_LINK_FLAGS}")
    endif()

    # target_compile_options(${plugBuildName} PRIVATE ${plugindefs})
    target_compile_options(${plugBuildName} PRIVATE ${BUILD_WARNING_FLAGS})
    target_compile_definitions(${plugBuildName} PRIVATE ${plugindefs})
    target_include_directories(${plugBuildName} PRIVATE src)
    
    if (NOT WIN32)
      target_link_options(${plugBuildName} PUBLIC -Wl,--no-undefined)
    endif()
    set_target_properties(${plugBuildName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_VST2_OUTPUT_PATH}$<0:...>)
    set_target_properties(${plugBuildName} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_VST2_OUTPUT_PATH}$<0:...>)
  ENDFUNCTION(FN_DL_PLUGIN_BUILD)

  if (PROJECT_BUILD_PLUGINS)
    if (NOT WIN32)
      set_target_properties(${LINK_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
      set_target_properties(slowstacktrace PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()

    set(SRC_FILES_VST2_PLUGIN
      ${MAIN_SRC_PATH}/plugins/plugin-base.cpp
      ${MAIN_SRC_PATH}/plugins/vst2wrapper/vst2wrapper.cpp
      ${MAIN_SRC_PATH}/thirdparty/vstsdk-plugin-2.4/vstplugmain.cpp
      ${MAIN_SRC_PATH}/plugins/plugin-window.cpp 
      ${MAIN_SRC_PATH}/plugins/plugincontrol.cpp)
    FN_DL_PLUGIN_BUILD(PLUGIN_GAIN_VST2 "gain-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUGIN_TYPE_GAIN")
    FN_DL_PLUGIN_BUILD(PLUGIN_EMPTY_VST2 "empty-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUGIN_TYPE_EMPTY")
    FN_DL_PLUGIN_BUILD(PLUGIN_LATENCY_VST2 "latency-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUGIN_TYPE_LATENCY")
    FN_DL_PLUGIN_BUILD(PLUGIN_SAMPLE_DELAY_VST2 "sample-delay-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUGIN_TYPE_SAMPLE_DELAY")
    FN_DL_PLUGIN_BUILD(PLUGIN_SAMPLE_CRUSH_VST2 "sample-crush-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUGIN_TYPE_SAMPLE_CRUSH")
    FN_DL_PLUGIN_BUILD(PLUGIN_STEREO_WIDTH_VST2 "stereo-width-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUGIN_TYPE_STEREO_WIDTH")

  endif()
endif()

if (WIN32)
  target_precompile_headers(${LINK_TARGET} PUBLIC <windows.h>)
endif (WIN32)

target_precompile_headers(${LINK_TARGET} PUBLIC 
  "$<$<COMPILE_LANGUAGE:CXX>:<cstddef$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<functional$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<array$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:<fstream$<ANGLE-R>>"
  "$<$<COMPILE_LANGUAGE:CXX>:include/types.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:include/math/vec.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:include/math/mat.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:include/track_impl.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:include/basectrl.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:host/mainctrl.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:include/fileio.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:include/renderresources.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:gui/gui.h>"
)

target_link_libraries(${LINK_TARGET} PUBLIC glfw)
target_link_libraries(${LINK_TARGET} PUBLIC SQLiteCpp)
target_link_libraries(${LINK_TARGET} PUBLIC PortAudio::PortAudio)
target_link_libraries(${LINK_TARGET} PUBLIC PortMidi::portmidi)
target_link_libraries(${LINK_TARGET} PUBLIC kissfft::kissfft-float)
target_link_libraries(${LINK_TARGET} PUBLIC ${SOXR_LIB})
target_link_libraries(${LINK_TARGET} PUBLIC slowstacktrace)

if (LINUX)
  target_link_libraries(${LINK_TARGET} PUBLIC Threads::Threads)
  target_link_libraries(${LINK_TARGET} PUBLIC ${X11_X11_LIB} ${CMAKE_DL_LIBS} ${GTK3_LIBRARIES} GL m)
endif (LINUX)

if (APPLE)
  target_link_libraries(${LINK_TARGET} PUBLIC
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
    m
  )
endif (APPLE)

if (WIN32)
  set_target_properties(${LINK_TARGET} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  target_link_libraries(${LINK_TARGET} PUBLIC user32 advapi32 shell32 dbghelp winmm opengl32 kernel32 gdi32 ole32 uuid comdlg32 setupapi mswsock ws2_32)
endif (WIN32)

if (WIN32 AND PROJECT_CFG_USE_STACK_PROTECTOR)
  target_link_libraries(${LINK_TARGET} PUBLIC ssp)
endif()

CONFIGURE_TARGET_DEPS(${LINK_TARGET})
GENERATE_TARGET_BUILD_INFO(${LINK_TARGET})

if (PROJECT_BUILD_BENCHMARKS)
  ADD_SUBDIRECTORY(benchmark)
endif()

if (PROJECT_BUILD_TESTS)
  ENABLE_TESTING()
  ADD_SUBDIRECTORY(test)
endif()

if (WIN32 AND NOT MSVC AND PROJECT_BUILD_DAW AND PROJECT_BUILD_SCANNER)
  find_program(NSIS_MAKE NAMES makensis PATH_SUFFIXES NSIS)
  if (NOT NSIS_MAKE)
    message(STATUS "Skip windows-installer: makensis not found")
  else()
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} ARGS -print-file-name=libc++.a OUTPUT_VARIABLE PATH_RUNTIMES)
    get_filename_component(PATH_RUNTIMES "${PATH_RUNTIMES}" PATH)
    get_filename_component(PATH_RUNTIMES "${PATH_RUNTIMES}" PATH)
    set(PATH_RUNTIMES "${PATH_RUNTIMES}/bin")
    add_custom_target(windows-installer
      DEPENDS DAW vstscanner
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND ${CMAKE_COMMAND} -E make_directory dist/bin
      COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:DAW> dist/bin/DAW.debug
      COMMAND ${CMAKE_OBJCOPY} --strip-all $<TARGET_FILE:DAW> dist/bin/DAW${CMAKE_EXECUTABLE_SUFFIX}
      COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=dist/bin/DAW.debug dist/bin/DAW${CMAKE_EXECUTABLE_SUFFIX}
      COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:vstscanner> dist/bin/vstscanner.debug
      COMMAND ${CMAKE_OBJCOPY} --strip-all $<TARGET_FILE:vstscanner> dist/bin/vstscanner${CMAKE_EXECUTABLE_SUFFIX}
      COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=dist/bin/vstscanner.debug dist/bin/vstscanner${CMAKE_EXECUTABLE_SUFFIX}
      COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_DEPS_INSTALL_PATH}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}${CMAKE_SHARED_MODULE_SUFFIX} dist/bin/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}${CMAKE_SHARED_MODULE_SUFFIX}
      COMMAND ${CMAKE_COMMAND} -E copy ${PATH_RUNTIMES}/libc++${CMAKE_SHARED_MODULE_SUFFIX} dist/bin/libc++${CMAKE_SHARED_MODULE_SUFFIX}
      # COMMAND ${CMAKE_COMMAND} -E copy ${PATH_RUNTIMES}/libunwind${CMAKE_SHARED_MODULE_SUFFIX} dist/bin/libunwind${CMAKE_SHARED_MODULE_SUFFIX}
      COMMAND ${NSIS_MAKE} /DAPP_VERSION=${CMAKE_PROJECT_VERSION} /DPRODUCT_VERSION=${CMAKE_PROJECT_VERSION}.0 /DPROJ_LOC=${CMAKE_CURRENT_SOURCE_DIR} /DDIST_LOC=${CMAKE_CURRENT_SOURCE_DIR}/dist/bin installer/NSIS-build-installer.nsi
    )
  endif()
endif()
