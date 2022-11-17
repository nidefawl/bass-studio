cmake_minimum_required(VERSION 3.21)
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(PROJECT_NAME "DAW" CACHE STRING "Project name")
set(PRODUCT_NAME "DAW" CACHE STRING "Product name")
set(CMAKE_USER_MAKE_RULES_OVERRIDE FixCMakeInitFlags)
project(${PROJECT_NAME} LANGUAGES C CXX VERSION 0.5.11)
include(GetGitRevision)
include(CompilerConfig)
include(BuildConfig)
include(WarningFlags)
include(FindLibraries)

option(PROJECT_BUILD_DAW "Build daw" ON)
option(PROJECT_BUILD_SCANNER "Build plugin scanner" ON)
option(PROJECT_BUILD_CLI_HOST "Build command line host" ON)
option(PROJECT_BUILD_TESTS "Build tests (headless)" ON)
option(PROJECT_BUILD_TESTS_UI "Build tests (requiring GL/Audio)" ON)
option(PROJECT_BUILD_BENCHMARKS "Build benchmarks" ON)
option(PROJECT_BUILD_PLUGINS "Build standalone plugins" ON)
option(PROJECT_CFG_USE_OPENGL3 "Use OpenGL 3.x instead of OpenGL 2" ON)
option(PROJECT_FORCE_LIB "Force building a lib" ON)
option(PROJECT_GENERATE_LINKER_MAP "Generate <build-dir>/<exe>_linker.map" OFF)
set(PROJECT_VST2_OUTPUT_PATH "${ABS_WORKING_DIR}" CACHE STRING "VST2 output path")
set(PROJECT_INSTALLER_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/installer" CACHE STRING "Installer output path")

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
  if (f MATCHES "/main_.*\.cpp")
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
    OR f MATCHES "/main_.*"
    OR f MATCHES ".*platform/linux/.*")
    set_source_files_properties(${f} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
  endif()
endforeach ()


if (MINGW)
  # The SEH assembly will not work correctly on clang mingw if any level of optimization is applied
  set(SRC_FILES_NO_OPTIMIZE src/host/plugin/vst/vstplugin-dispatch.cpp)
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
message(STATUS "PROJECT_INSTALLER_OUTPUT_PATH ${PROJECT_INSTALLER_OUTPUT_PATH}")
message(STATUS "PROJECT_BUILD_DAW ${PROJECT_BUILD_DAW}")
message(STATUS "PROJECT_BUILD_TESTS ${PROJECT_BUILD_TESTS} ")
message(STATUS "PROJECT_BUILD_BENCHMARKS ${PROJECT_BUILD_BENCHMARKS} ")
message(STATUS "PROJECT_BUILD_SCANNER ${PROJECT_BUILD_SCANNER} ")
message(STATUS "PROJECT_BUILD_CLI_HOST ${PROJECT_BUILD_CLI_HOST}")
message(STATUS "PROJECT_BUILD_PLUGINS ${PROJECT_BUILD_PLUGINS}")
message(STATUS "BUILD WARNINGS ${BUILD_WARNING_FLAGS}")

message(STATUS "PROJECT_CFG_NO_OMIT_FRAME_POINTER ${PROJECT_CFG_NO_OMIT_FRAME_POINTER}")

if (PROJECT_BUILD_WITH_SANITIZER)
  message(WARNING "Building with sanitizer: ${PROJECT_CFG_FSANITIZE}")
endif()

message(STATUS "PROJECT_PRODUCT_NAME ${PROJECT_PRODUCT_NAME}")
message(STATUS "PRODUCT_HOST_NAME ${PRODUCT_HOST_NAME}")
message(STATUS "PROJECT_BINARY_NAME ${PROJECT_BINARY_NAME}")

message(STATUS "PRODUCT_VERSION ${PRODUCT_VERSION}")

message(STATUS "PROJECT_VENDOR_NAME ${PROJECT_VENDOR_NAME}")
message(STATUS "PRODUCT_COPYRIGHT ${PRODUCT_COPYRIGHT}")
message(STATUS "PRODUCT_URL_DOCS ${PRODUCT_URL_DOCS}")
message(STATUS "PRODUCT_URL_VENDOR ${PRODUCT_URL_VENDOR}")

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
  add_executable(${PROJECT_NAME} "src/host/daw/main_daw.cpp"
                ${RESOURCE_FILES} 
                ${CPP_SRC_FILES} 
                ${C_SRC_FILES} 
                ${MM_SRC_FILES} 
                ${PROJECT_DEPS_SRC_FILES} 
                ${HDR_FILES})
  CONFIGURE_TARGET_OUTPUT(${PROJECT_NAME} ${PROJECT_BINARY_NAME} ${PROJECT_PRODUCT_NAME})
endif()


target_compile_definitions(${LINK_TARGET} PUBLIC GLFW_INCLUDE_NONE=1 USE_WIN32_EXC_HOOKS BUILD_DAW_HOST=1)
#target_compile_definitions(${LINK_TARGET} PUBLIC PA_ENABLE_DEBUG_OUTPUT=1)
target_compile_definitions(${LINK_TARGET} PUBLIC GLM_FORCE_SSE42=1 VST_FORCE_DEPRECATED=0 MUPARSER_STATIC=1 LIBARCHIVE_STATIC=1)

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
  MACRO(DEFINE_EXECUTABLE target_name programDisplayName target_sources)
    source_group("src" FILES ${target_sources})
    add_executable(${target_name} ${target_sources})
    target_link_libraries(${target_name} PRIVATE "dawlib")
    target_compile_options(${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
    CONFIGURE_TARGET_OUTPUT(${target_name} ${target_name} ${programDisplayName})
  ENDMACRO()

  # define main executable
  if (PROJECT_BUILD_DAW)
    DEFINE_EXECUTABLE(${PROJECT_BINARY_NAME} ${PROJECT_PRODUCT_NAME} "src/host/daw/main_daw.cpp;${RESOURCE_FILES}")
  endif()

  if (PROJECT_BUILD_SCANNER)
    DEFINE_EXECUTABLE("pluginscanner" "${PROJECT_PRODUCT_NAME} Pluginscanner" "src/host/pluginscanner/main_scanner.cpp")
  endif()

  if (PROJECT_BUILD_CLI_HOST)
    DEFINE_EXECUTABLE("host" "${PROJECT_PRODUCT_NAME} Command Line Host" "src/host/cli/main_cmdline.cpp")
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
    CONFIGURE_TARGET_OUTPUT(${plugBuildName} ${plugBuildName} ${plugBuildOutputName})

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

  if (PROJECT_BUILD_PLUGINS AND NOT PROJECT_BUILD_WITH_SANITIZER)
    if (NOT WIN32)
      set_target_properties(${LINK_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
      set_target_properties(slowstacktrace PROPERTIES POSITION_INDEPENDENT_CODE ON)
      set_target_properties(libarchive PROPERTIES POSITION_INDEPENDENT_CODE ON)
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
    FN_DL_PLUGIN_BUILD(PLUGIN_SYNTH_VST2 "synth-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUG_INT_SYNTH")
    FN_DL_PLUGIN_BUILD(PLUGIN_HOSTINFO_VST2 "hostinfo-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUG_INT_HOSTINFO")

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
  "$<$<COMPILE_LANGUAGE:CXX>:host/track/track_impl.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:include/basectrl.h>"
  "$<$<COMPILE_LANGUAGE:CXX>:host/daw/mainctrl.h>"
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
target_link_libraries(${LINK_TARGET} PUBLIC libarchive)

if (ZLIB_DEBUG AND ZLIB_RELEASE)
  target_link_libraries(${LINK_TARGET} PUBLIC optimized ${ZLIB_RELEASE} debug ${ZLIB_DEBUG})
elseif(ZLIB_RELEASE)
  target_link_libraries(${LINK_TARGET} PUBLIC ${ZLIB_RELEASE})
else(ZLIB_DEBUG)
  target_link_libraries(${LINK_TARGET} PUBLIC ${ZLIB_DEBUG})
endif()

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

if (PROJECT_BUILD_DAW AND PROJECT_BUILD_SCANNER)
  set(PATH_DIST "${CMAKE_BINARY_DIR}/dist/bin")
  # clean dist/bin directory
  file(REMOVE_RECURSE "${PATH_DIST}")
  add_custom_target(dist-strip
    VERBATIM
    DEPENDS ${PROJECT_BINARY_NAME} pluginscanner
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND}
      -E make_directory "${PATH_DIST}"
    COMMAND ${CMAKE_COMMAND}
      -E make_directory "${PROJECT_INSTALLER_OUTPUT_PATH}"
    COMMAND ${CMAKE_OBJCOPY}
      --only-keep-debug "$<TARGET_FILE:${PROJECT_BINARY_NAME}>" "${PATH_DIST}/${PROJECT_BINARY_NAME}.debug"
    COMMAND ${CMAKE_OBJCOPY}
      --strip-all "$<TARGET_FILE:${PROJECT_BINARY_NAME}>" "${PATH_DIST}/${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
    COMMAND chrpath -r '$ORIGIN' "${PATH_DIST}/${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
    COMMAND ${CMAKE_OBJCOPY}
      "--add-gnu-debuglink=${PATH_DIST}/${PROJECT_BINARY_NAME}.debug" "${PATH_DIST}/${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
    COMMAND ${CMAKE_OBJCOPY}
      --only-keep-debug "$<TARGET_FILE:pluginscanner>" "${PATH_DIST}/pluginscanner.debug"
    COMMAND ${CMAKE_OBJCOPY}
      --strip-all "$<TARGET_FILE:pluginscanner>" "${PATH_DIST}/pluginscanner${CMAKE_EXECUTABLE_SUFFIX}"
    COMMAND chrpath -r '$ORIGIN' "${PATH_DIST}/pluginscanner${CMAKE_EXECUTABLE_SUFFIX}"
    COMMAND ${CMAKE_OBJCOPY}
      "--add-gnu-debuglink=${PATH_DIST}/pluginscanner.debug" "${PATH_DIST}/pluginscanner${CMAKE_EXECUTABLE_SUFFIX}"
  )
  if (LINUX AND CLANG)
    set(PROJECT_INSTALLER_NAME_UNIX "${PROJECT_BINARY_NAME}-v${PRODUCT_VERSION}-linux-x86_64")
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} ARGS -print-file-name=libc++.a OUTPUT_VARIABLE PATH_RUNTIMES)
    get_filename_component(PATH_RUNTIMES "${PATH_RUNTIMES}" PATH)
    message(STATUS "PATH_RUNTIMES: ${PATH_RUNTIMES}")
    set(LIST_TARGETS_COPY "dist-copy-libs")
    add_custom_target(dist-copy-libs
      VERBATIM
      DEPENDS dist-strip
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} 
        -E copy
        "${PROJECT_DEPS_INSTALL_PATH}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.so.0"
        "${PATH_DIST}/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.so.0"
    )
    # this is a bit akward: copy files only if they exist
    # copy ${PATH_RUNTIMES}/libc++abi.so.1 if exists
    if(EXISTS "${PATH_RUNTIMES}/libc++abi.so.1")
      add_custom_target(dist-copy-runtimes-clang-linux1
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} 
          -E copy
          "${PATH_RUNTIMES}/libc++abi.so.1"
          "${PATH_DIST}/libc++abi.so.1"
      )
      list(APPEND LIST_TARGETS_COPY "dist-copy-runtimes-clang-linux1")
    endif()
    # copy ${PATH_RUNTIMES}/libc++.so.2 if exists
    if(EXISTS "${PATH_RUNTIMES}/libc++.so.2")
      add_custom_target(dist-copy-runtimes-clang-linux2
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} 
          -E copy
          "${PATH_RUNTIMES}/libc++.so.2"
          "${PATH_DIST}/libc++.so.2"
      )
      list(APPEND LIST_TARGETS_COPY "dist-copy-runtimes-clang-linux2")
    elseif(EXISTS "${PATH_RUNTIMES}/libc++.so.1")
      add_custom_target(dist-copy-runtimes-clang-linux2
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} 
          -E copy
          "${PATH_RUNTIMES}/libc++.so.1"
          "${PATH_DIST}/libc++.so.1"
      )
      list(APPEND LIST_TARGETS_COPY "dist-copy-runtimes-clang-linux2")
    endif()

    # copy the following list
    # res/fonts -> dist/bin/res/fonts
    # res/icons -> dist/bin/res/icons
    # res/cursors -> dist/bin/res/cursors
    # res/*.png -> dist/bin/res/

    add_custom_target(dist-copy-resources
      VERBATIM
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMAND ${CMAKE_COMMAND} 
        -E copy_directory
        "${PROJECT_SOURCE_DIR}/res/fonts"
        "${PATH_DIST}/res/fonts"
      COMMAND ${CMAKE_COMMAND} 
        -E copy_directory
        "${PROJECT_SOURCE_DIR}/res/icons"
        "${PATH_DIST}/res/icons"
      COMMAND ${CMAKE_COMMAND} 
        -E copy_directory
        "${PROJECT_SOURCE_DIR}/res/cursors"
        "${PATH_DIST}/res/cursors"
      COMMAND ${CMAKE_COMMAND} 
        -E copy
        "${PROJECT_SOURCE_DIR}/res/led.png"
        "${PATH_DIST}/res/"
      COMMAND ${CMAKE_COMMAND} 
        -E copy
        "${PROJECT_SOURCE_DIR}/res/led_off.png"
        "${PATH_DIST}/res/"
      COMMAND ${CMAKE_COMMAND} 
        -E copy
        "${PROJECT_SOURCE_DIR}/res/led_glow.png"
        "${PATH_DIST}/res/"
    )
    list(APPEND LIST_TARGETS_COPY "dist-copy-resources")
    add_custom_target(dist-installer
      VERBATIM
      DEPENDS dist-strip ${LIST_TARGETS_COPY}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist
      COMMAND tar "--exclude=*.debug" "-cf" "${PROJECT_INSTALLER_OUTPUT_PATH}/${PROJECT_INSTALLER_NAME_UNIX}.tar.gz" "--transform" "s%^bin%${PROJECT_INSTALLER_NAME_UNIX}%" bin
    )
  elseif(WIN32 AND CLANG AND MINGW)
    find_program(NSIS_MAKE NAMES makensis PATH_SUFFIXES NSIS)
    if (NOT NSIS_MAKE)
      message(STATUS "Skip dist-installer: makensis not found")
    else()
      execute_process(COMMAND ${CMAKE_CXX_COMPILER} ARGS -print-file-name=libc++.a OUTPUT_VARIABLE PATH_RUNTIMES)
      get_filename_component(PATH_RUNTIMES "${PATH_RUNTIMES}" PATH)
      get_filename_component(PATH_RUNTIMES "${PATH_RUNTIMES}" PATH) # remove /lib
      set(PATH_RUNTIMES "${PATH_RUNTIMES}/bin")
      add_custom_target(dist-copy-runtimes-clang-win32
        VERBATIM
        DEPENDS dist-strip
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} 
          -E copy
          "${PROJECT_DEPS_INSTALL_PATH}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}${CMAKE_SHARED_MODULE_SUFFIX}"
          "${PATH_DIST}/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}${CMAKE_SHARED_MODULE_SUFFIX}"
        # COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_DEPS_INSTALL_PATH}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}archive_release${CMAKE_SHARED_MODULE_SUFFIX} ${PATH_DIST}/${CMAKE_SHARED_LIBRARY_PREFIX}archive_release${CMAKE_SHARED_MODULE_SUFFIX}
        COMMAND ${CMAKE_COMMAND} 
          -E copy
          "${PATH_RUNTIMES}/libc++${CMAKE_SHARED_MODULE_SUFFIX}"
          "${PATH_DIST}/libc++${CMAKE_SHARED_MODULE_SUFFIX}"
        # COMMAND ${CMAKE_COMMAND} -E copy ${PATH_RUNTIMES}/libunwind${CMAKE_SHARED_MODULE_SUFFIX} ${PATH_DIST}/libunwind${CMAKE_SHARED_MODULE_SUFFIX}
      )
      add_custom_target(dist-installer
        VERBATIM
        DEPENDS dist-copy-runtimes-clang-win32
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${NSIS_MAKE}
          "-V4"
          "-DPROJECT_INSTALLER_OUTPUT_PATH=${PROJECT_INSTALLER_OUTPUT_PATH}"
          "-DPROJ_LOC=${CMAKE_CURRENT_SOURCE_DIR}"
          "-DDIST_LOC=${PATH_DIST}"
          "-DPROJECT_BINARY_NAME=${PROJECT_BINARY_NAME}"
          "-DPROJECT_BINARY_PATH=${PATH_DIST}/${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
          "-DPRODUCT_NAME_DISPLAY=${PROJECT_PRODUCT_NAME}"
          "-DPROJECT_VENDOR_NAME=${PROJECT_VENDOR_NAME}"
          "-DPRODUCT_URL_VENDOR=${PRODUCT_URL_VENDOR}"
          "-DPRODUCT_COPYRIGHT=${PRODUCT_COPYRIGHT}"
          "-DPRODUCT_VERSION=${PRODUCT_VERSION}"
          "-DPRODUCT_VERSION_MAJOR_MINOR_PATCH_REVISION=${PRODUCT_VERSION_MAJOR_MINOR_PATCH_REVISION}"
          "installer/NSIS-build-installer.nsi"
      )
    endif ()
  else()
    message(STATUS "Skip dist-installer: Platform not supported: ${CMAKE_SYSTEM_NAME} ${CMAKE_CXX_COMPILER_ID}")
  endif()
endif()
