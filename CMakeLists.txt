cmake_minimum_required(VERSION 3.21)
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(PROJECT_NAME "bass" CACHE STRING "Project name")
set(PROJECT_PRODUCT_NAME "Bass Studio" CACHE STRING "Product name")
set(PROJECT_VENDOR_NAME "Michael Hept" CACHE STRING "Vendor name")
set(CMAKE_USER_MAKE_RULES_OVERRIDE FixCMakeInitFlags)
project(${PROJECT_NAME} LANGUAGES C CXX VERSION 0.6.3)
if (APPLE) 
  enable_language(OBJC)
  enable_language(OBJCXX)
endif ()
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
set(PROJECT_INSTALLER_OUTPUT_PATH "${CMAKE_BINARY_DIR}/installer" CACHE STRING "Installer output path")

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
  if (f MATCHES "src/thirdparty/vst3sdk/.*")
    continue()
  endif()
  if (UNIX AND f MATCHES "/platform/win/")
    continue()
  endif()
  if (WIN32 AND f MATCHES "/platform/linux/")
    continue()
  endif()
  if (APPLE AND f MATCHES "/platform/linux/nfd/")
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
  if (f MATCHES "src/thirdparty/vst3sdk/.*")
    continue()
  endif()
  if (UNIX AND f MATCHES "/platform/win/")
    continue()
  endif()
  if (WIN32 AND f MATCHES "/platform/linux/")
    continue()
  endif()
  if (APPLE AND f MATCHES "/platform/linux/nfd/")
    continue()
  endif()
  if (MSVC AND f MATCHES "/platform/mingw/")
    continue()
  endif()
  list(APPEND CPP_SRC_FILES ${f})
endforeach ()

set_source_files_properties(${MM_SRC_FILES} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

foreach (f ${CPP_ALL_FILES})
  if (f MATCHES ".*host/plugin/.*"
    OR f MATCHES ".*.mm"
    OR f MATCHES "/main_.*"
    OR f MATCHES ".*platform/linux/.*"
    OR f MATCHES ".*/plugins/synth/synth-plugin.cpp"
    OR f MATCHES ".*/util/fp_math.cpp"
    OR f MATCHES ".*/gui/linetess/linetess.cpp"
    OR f MATCHES ".*/gui/linetess/pymachine.cpp"
    OR f MATCHES ".*/plugins/eq/eq-plugin.cpp"
    OR f MATCHES ".*/gui/clipeditor/clipeditor.cpp")
    set_source_files_properties(${f} PROPERTIES 
        SKIP_UNITY_BUILD_INCLUSION ON
        SKIP_PRECOMPILE_HEADERS ON
    )
    message(STATUS "Skipping PCH/Unity for ${f}")
  endif()
endforeach ()

if (MINGW)
  # The SEH assembly will not work correctly on clang mingw if any level of optimization is applied
  set(SRC_FILES_NO_OPTIMIZE src/host/plugin/vst/vstplugin-dispatch.cpp)
  set_source_files_properties(${SRC_FILES_NO_OPTIMIZE} PROPERTIES COMPILE_OPTIONS "-fno-lto;-mtune=generic;-fno-function-sections;-fno-data-sections")
endif()

if (CLANG)
  set_source_files_properties(src/util/fp_math.cpp PROPERTIES
    COMPILE_OPTIONS "-fno-lto;-mtune=generic;-fno-function-sections;-fno-data-sections;-fno-fast-math;-O2"
  )
endif()
add_compile_definitions(TINYSPLINE_FLOAT_PRECISION=1)
include_directories(SYSTEM "${PROJECT_DEPS_PATH}/tinyspline/src/")
set(PROJECT_DEP_TINYSPLINE_SRC_FILES
  tinyspline/src/parson.c
  tinyspline/src/tinyspline.c
  tinyspline/src/tinysplinecxx.cxx
)
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
  ${PROJECT_DEP_TINYSPLINE_SRC_FILES}
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
target_compile_definitions(${LINK_TARGET} PUBLIC GLM_FORCE_SSE42=1 GLM_ENABLE_EXPERIMENTAL=1 VST_FORCE_DEPRECATED=0 MUPARSER_STATIC=1 LIBARCHIVE_STATIC=1)
if (PROJECT_BUILD_WITH_SANITIZER)
  target_compile_definitions(${LINK_TARGET} PUBLIC BUILD_USE_SANITIZER=1)
endif()

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
    
    if (LINUX)
      target_link_options(${plugBuildName} PUBLIC -Wl,--no-undefined)
    endif()
    set_target_properties(${plugBuildName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_VST2_OUTPUT_PATH}$<0:...>)
    set_target_properties(${plugBuildName} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_VST2_OUTPUT_PATH}$<0:...>)
  ENDFUNCTION(FN_DL_PLUGIN_BUILD)

  if (PROJECT_BUILD_PLUGINS AND NOT PROJECT_BUILD_WITH_SANITIZER)
    if (NOT WIN32)
      set_target_properties(${LINK_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
      if (TARGET slowstacktrace)
        set_target_properties(slowstacktrace PROPERTIES POSITION_INDEPENDENT_CODE ON)
      endif()
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
    FN_DL_PLUGIN_BUILD(PLUGIN_SYNTH_VST2 "synth-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUGIN_TYPE_SYNTH")
    FN_DL_PLUGIN_BUILD(PLUGIN_HOSTINFO_VST2 "hostinfo-vst" "${SRC_FILES_VST2_PLUGIN}" "BUILD_EXTERNAL_VST2_PLUGIN=PLUGIN_TYPE_HOSTINFO")

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
if (TARGET slowstacktrace)
  target_link_libraries(${LINK_TARGET} PUBLIC slowstacktrace)
  target_compile_definitions(${LINK_TARGET} PUBLIC HAVE_SLOWSTACKTRACE=1)
else()
  message(WARNING "slowstacktrace not found")
endif()
if (TARGET pybind11::embed)
  target_link_libraries(${LINK_TARGET} PUBLIC pybind11::embed)
  target_compile_definitions(${LINK_TARGET} PUBLIC HAVE_PYTHON_INTERPRETER=1)
else()
  message(WARNING "pybind11::embed not found")
  target_compile_definitions(${LINK_TARGET} PUBLIC HAVE_PYTHON_INTERPRETER=0)
endif()

target_link_libraries(${LINK_TARGET} PUBLIC zlib)
target_link_libraries(${LINK_TARGET} PUBLIC libarchive)

# VST 3 SDK
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF)
set(SMTG_RENAME_ASSERT OFF)
add_subdirectory(src/thirdparty/vst3sdk)
target_link_libraries(${LINK_TARGET} PUBLIC sdk_hosting)

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
  set(PATH_DIST "${CMAKE_BINARY_DIR}/dist")
  set(PATH_RES "${PATH_DIST}/res")
  if (APPLE)
    set(PATH_RES "${PATH_DIST}/Resources/res")
  endif()
  set(PATH_BIN "${PATH_DIST}")
  if (APPLE)
    set(PATH_BIN "${PATH_DIST}/MacOS")
  endif()
  set(PATH_BINARY_MAIN_STRIPPED "${PATH_BIN}/${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
  set(PATH_BINARY_SCANNER_STRIPPED "${PATH_BIN}/daw-pluginscanner${CMAKE_EXECUTABLE_SUFFIX}")
  add_custom_target(dist-strip
    ALL
    VERBATIM
    BYPRODUCTS ${PATH_BINARY_MAIN_STRIPPED} ${PATH_BINARY_SCANNER_STRIPPED}
    DEPENDS ${PROJECT_BINARY_NAME} pluginscanner
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PATH_DIST}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_INSTALLER_OUTPUT_PATH}"
    COMMAND ${CMAKE_OBJCOPY}
      --only-keep-debug "$<TARGET_FILE:${PROJECT_BINARY_NAME}>" "${PATH_DIST}/${PROJECT_BINARY_NAME}.debug"
    COMMAND ${CMAKE_OBJCOPY}
      --strip-all "$<TARGET_FILE:${PROJECT_BINARY_NAME}>" "${PATH_BINARY_MAIN_STRIPPED}"
    COMMAND ${CMAKE_OBJCOPY}
      "--add-gnu-debuglink=${PATH_DIST}/${PROJECT_BINARY_NAME}.debug" "${PATH_BINARY_MAIN_STRIPPED}"
    COMMAND ${CMAKE_OBJCOPY}
      --only-keep-debug "$<TARGET_FILE:pluginscanner>" "${PATH_DIST}/daw-pluginscanner.debug"
    COMMAND ${CMAKE_OBJCOPY}
      --strip-all "$<TARGET_FILE:pluginscanner>" "${PATH_BINARY_SCANNER_STRIPPED}"
    COMMAND ${CMAKE_OBJCOPY}
      "--add-gnu-debuglink=${PATH_DIST}/daw-pluginscanner.debug" "${PATH_BINARY_SCANNER_STRIPPED}"
  )
  add_custom_target(dist-create-defaults-zip
    VERBATIM
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/dist/defaults"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PATH_DIST}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PATH_DIST}/defaults"
    COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${PATH_DIST}/defaults/data.zip" --format=zip .)

  # copy the following list
  # res/fonts -> dist/res/fonts
  # res/icons -> dist/res/icons
  # res/cursors -> dist/res/cursors
  # res/*.png -> dist/res/
  add_custom_target(dist-copy-resources
    ALL
    VERBATIM
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PATH_RES}"
    COMMAND ${CMAKE_COMMAND} 
      -E copy_directory
      "${PROJECT_SOURCE_DIR}/res/fonts"
      "${PATH_RES}/fonts"
    COMMAND ${CMAKE_COMMAND} 
      -E copy_directory
      "${PROJECT_SOURCE_DIR}/res/icons"
      "${PATH_RES}/icons"
    COMMAND ${CMAKE_COMMAND} 
      -E copy_directory
      "${PROJECT_SOURCE_DIR}/res/cursors"
      "${PATH_RES}/cursors"
    COMMAND ${CMAKE_COMMAND} 
      -E copy
      "${PROJECT_SOURCE_DIR}/res/led.png"
      "${PATH_RES}"
    COMMAND ${CMAKE_COMMAND} 
      -E copy
      "${PROJECT_SOURCE_DIR}/res/led_off.png"
      "${PATH_RES}"
    COMMAND ${CMAKE_COMMAND} 
      -E copy
      "${PROJECT_SOURCE_DIR}/res/led_glow.png"
      "${PATH_RES}"
  )
  if (APPLE AND CLANG)
    set(PROJECT_INSTALLER_NAME_APPLE "${PROJECT_BINARY_NAME}-v${PRODUCT_VERSION}-macos-x86_64")
    set(LIST_RUNTIMES "")
    if(EXISTS "${PROJECT_DEPS_INSTALL_PATH}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.0.dylib")
      list(APPEND LIST_RUNTIMES "${PROJECT_DEPS_INSTALL_PATH}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.0.dylib")
    endif()
    set(PROJECT_MACOS_BUNDLE_ID "${PROJECT_BINARY_NAME}")
    set(PROJECT_MACOS_BUNDLE_EXECUTABLE "${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}")
    set(ICON_FILE "${PROJECT_SOURCE_DIR}/res/${PROJECT_BINARY_NAME}.icns")
    if (NOT EXISTS "${ICON_FILE}")
      set(ICON_FILE "${PROJECT_SOURCE_DIR}/res/DAW.icns")
    endif()
    set(PROJECT_MACOS_BUNDLE_ICON_FILE "${PROJECT_BINARY_NAME}.icns")
    configure_file(
      "${MAIN_SRC_PATH}/Info.plist.in"
      "${PATH_DIST}/Info.plist"
      NEWLINE_STYLE LF
    )
    set(PROJECT_INSTALLER_ARCHIVE "${PROJECT_INSTALLER_OUTPUT_PATH}/${PROJECT_INSTALLER_NAME_APPLE}.zip")
    message(STATUS "LIST_RUNTIMES: ${LIST_RUNTIMES}")
    set(PATH_PACK_MACOS "${CMAKE_BINARY_DIR}/pack/${PROJECT_INSTALLER_NAME_APPLE}.app")
    add_custom_target(dist-create-macos-bundle
      VERBATIM
      DEPENDS dist-strip dist-copy-resources dist-create-defaults-zip
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMAND rm -Rf ${CMAKE_BINARY_DIR}/pack 
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/pack"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${PATH_PACK_MACOS}/Contents"
      COMMAND ${CMAKE_COMMAND} -E copy "${PATH_DIST}/Info.plist" "${PATH_PACK_MACOS}/Contents"
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${PATH_DIST}/MacOS" "${PATH_PACK_MACOS}/Contents/MacOS"
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${PATH_DIST}/Resources" "${PATH_PACK_MACOS}/Contents/Resources"
      COMMAND ${CMAKE_COMMAND} 
        -E copy_directory
        "${PROJECT_SOURCE_DIR}/dist/docs"
        "${PATH_PACK_MACOS}/Contents/Resources/docs"
      COMMAND ${CMAKE_COMMAND} 
        -E copy
        "${PATH_DIST}/defaults/data.zip"
        "${PATH_PACK_MACOS}/Contents/Resources/defaults/data.zip"
      COMMAND ${CMAKE_COMMAND} -E copy "${ICON_FILE}" "${PATH_PACK_MACOS}/Contents/Resources/${PROJECT_BINARY_NAME}.icns"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${PATH_PACK_MACOS}/Contents/Frameworks"
      COMMAND ${CMAKE_COMMAND} -E copy ${LIST_RUNTIMES} "${PATH_PACK_MACOS}/Contents/Frameworks"
      COMMAND install_name_tool
        -id
        "@executable_path/../Frameworks/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.0.dylib"
        "${PATH_PACK_MACOS}/Contents/Frameworks/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.0.dylib"
      COMMAND install_name_tool
        -change 
        "${PROJECT_DEPS_INSTALL_PATH}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.0.dylib"
        "@executable_path/../Frameworks/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.0.dylib"
        "${PATH_PACK_MACOS}/Contents/MacOS/${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
      COMMAND install_name_tool
        -change 
        "${PROJECT_DEPS_INSTALL_PATH}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.0.dylib"
        "@executable_path/../Frameworks/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.0.dylib"
        "${PATH_PACK_MACOS}/Contents/MacOS/daw-pluginscanner${CMAKE_EXECUTABLE_SUFFIX}"
    )
    add_custom_target(dist-compress-macos-bundle
      BYPRODUCTS ${PROJECT_INSTALLER_ARCHIVE}
      VERBATIM
      DEPENDS dist-create-macos-bundle
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/pack
      COMMAND zip -rq9 ${PROJECT_INSTALLER_ARCHIVE} .
      # COMMAND ${CMAKE_COMMAND} -E copy_directory . "${PROJECT_INSTALLER_OUTPUT_PATH}"
    )
    add_custom_target(dist-installer DEPENDS dist-compress-macos-bundle)
  elseif (LINUX AND CLANG)
    set(PROJECT_INSTALLER_NAME_UNIX "${PROJECT_BINARY_NAME}-v${PRODUCT_VERSION}-linux-x86_64")
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} ARGS -print-file-name=libc++.a OUTPUT_VARIABLE PATH_RUNTIMES)
    get_filename_component(PATH_RUNTIMES "${PATH_RUNTIMES}" PATH)
    message(STATUS "PATH_RUNTIMES: ${PATH_RUNTIMES}")
    set(LIST_RUNTIMES "")
    if(EXISTS "${PROJECT_DEPS_INSTALL_PATH}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.so.0")
      list(APPEND LIST_RUNTIMES "${PROJECT_DEPS_INSTALL_PATH}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}.so.0")
    endif()
    if(EXISTS "${PATH_RUNTIMES}/libc++abi.so.1")
      list(APPEND LIST_RUNTIMES "${PATH_RUNTIMES}/libc++abi.so.1")
    endif()
    if(EXISTS "${PATH_RUNTIMES}/libc++.so.2")
      list(APPEND LIST_RUNTIMES "${PATH_RUNTIMES}/libc++.so.2")
    endif()
    if(EXISTS "${PATH_RUNTIMES}/libc++.so.1")
      list(APPEND LIST_RUNTIMES "${PATH_RUNTIMES}/libc++.so.1")
    endif()

    set(PROJECT_INSTALLER_OUTPUT "${PROJECT_INSTALLER_OUTPUT_PATH}/${PROJECT_INSTALLER_NAME_UNIX}.tar.gz")
    add_custom_command(
      OUTPUT ${PROJECT_INSTALLER_OUTPUT} CMakeStinks.txt
      VERBATIM
      DEPENDS dist-strip dist-copy-resources dist-create-defaults-zip
      WORKING_DIRECTORY ${PATH_DIST}
      COMMAND ${CMAKE_COMMAND} 
        -E copy_directory
        "${PROJECT_SOURCE_DIR}/dist/docs"
        "${PATH_DIST}/docs"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${PATH_DIST}/lib"
      COMMAND ${CMAKE_COMMAND} 
        -E copy
        ${LIST_RUNTIMES}
        "${PATH_DIST}/lib/"
      COMMAND chrpath -r \$ORIGIN/lib "${PATH_DIST}/${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
      COMMAND chrpath -r \$ORIGIN/lib "${PATH_DIST}/daw-pluginscanner${CMAKE_EXECUTABLE_SUFFIX}"
      COMMAND tar "--exclude=*.debug" "-vczf" "${PROJECT_INSTALLER_OUTPUT}" "--transform" "s%^.%${PROJECT_INSTALLER_NAME_UNIX}%" .
    )
    add_custom_target(dist-installer DEPENDS ${PROJECT_INSTALLER_OUTPUT})
  elseif(WIN32 AND ((CLANG AND MINGW) OR MSVC))
    find_program(NSIS_MAKE NAMES makensis PATH_SUFFIXES NSIS)
    if (NOT NSIS_MAKE)
      message(STATUS "Skip dist-installer: makensis not found")
    else()
      if (CLANG AND MINGW)
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} ARGS -print-file-name=libc++.a OUTPUT_VARIABLE PATH_RUNTIMES)
        get_filename_component(PATH_RUNTIMES "${PATH_RUNTIMES}" PATH)
        get_filename_component(PATH_RUNTIMES "${PATH_RUNTIMES}" PATH) # remove /lib
        set(PATH_RUNTIMES "${PATH_RUNTIMES}/bin")
        set(DIST_COPY_RUNTIME_DEPENDS dist-strip)
        
        # if libunwind exists create a target to copy it
        if(EXISTS "${PATH_RUNTIMES}/libunwind${CMAKE_SHARED_MODULE_SUFFIX}")
          add_custom_target(dist-copy-libunwind
            VERBATIM
            DEPENDS dist-strip
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMAND ${CMAKE_COMMAND} 
              -E copy
              "${PATH_RUNTIMES}/libunwind${CMAKE_SHARED_MODULE_SUFFIX}"
              "${PATH_DIST}/libunwind${CMAKE_SHARED_MODULE_SUFFIX}"
          )
          set(DIST_COPY_RUNTIME_DEPENDS ${DIST_COPY_RUNTIME_DEPENDS} dist-copy-libunwind)
        endif()
        
        add_custom_target(dist-copy-runtimes
          VERBATIM
          DEPENDS ${DIST_COPY_RUNTIME_DEPENDS}
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${CMAKE_COMMAND} 
            -E copy
            "${PROJECT_DEPS_INSTALL_PATH}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}${CMAKE_SHARED_MODULE_SUFFIX}"
            "${PATH_DIST}/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}${CMAKE_SHARED_MODULE_SUFFIX}"
          COMMAND ${CMAKE_COMMAND} 
            -E copy
            "${PATH_RUNTIMES}/libc++${CMAKE_SHARED_MODULE_SUFFIX}"
            "${PATH_DIST}/libc++${CMAKE_SHARED_MODULE_SUFFIX}"
        )
        
      else()
      
        add_custom_target(dist-strip-msvc
          ALL
          VERBATIM
          BYPRODUCTS ${PATH_BINARY_MAIN_STRIPPED} ${PATH_BINARY_SCANNER_STRIPPED}
          DEPENDS ${PROJECT_BINARY_NAME} pluginscanner
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${CMAKE_COMMAND} -E make_directory "${PATH_DIST}"
          COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_INSTALLER_OUTPUT_PATH}"
          # just copy both files to dist
          COMMAND ${CMAKE_COMMAND} 
            -E copy
            "$<TARGET_FILE:${PROJECT_BINARY_NAME}>"
            "${PATH_DIST}/${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
          COMMAND ${CMAKE_COMMAND}
            -E copy
            "$<TARGET_FILE:pluginscanner>"
            "${PATH_DIST}/daw-pluginscanner${CMAKE_EXECUTABLE_SUFFIX}"
        )
        add_custom_target(dist-copy-runtimes
          VERBATIM
          DEPENDS dist-strip-msvc
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMAND ${CMAKE_COMMAND} 
            -E copy
            "${PROJECT_DEPS_INSTALL_PATH}/bin/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}${CMAKE_SHARED_MODULE_SUFFIX}"
            "${PATH_DIST}/${CMAKE_SHARED_LIBRARY_PREFIX}${SOXR_DYNLIB_NAME}${CMAKE_SHARED_MODULE_SUFFIX}"
        )
      endif()
      add_custom_target(dist-installer
        VERBATIM
        DEPENDS dist-copy-runtimes dist-create-defaults-zip
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${NSIS_MAKE}
          "-V4"
          "-DPROJECT_INSTALLER_OUTPUT_PATH=${PROJECT_INSTALLER_OUTPUT_PATH}"
          "-DPROJ_LOC=${CMAKE_CURRENT_SOURCE_DIR}"
          "-DDIST_LOC=${PATH_DIST}"
          "-DPROJECT_BINARY_NAME=${PROJECT_BINARY_NAME}"
          "-DPROJECT_MAIN_BINARY=${PROJECT_BINARY_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
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
