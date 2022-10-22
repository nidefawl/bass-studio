configure_file( TestBase.hpp.in ${CMAKE_CURRENT_LIST_DIR}/TestBase.hpp )

include(CheckCXXCompilerFlag)

set(TC_WARNING_FLAGS "")

add_compile_definitions("PROJECT_UNITTEST")

# Enable high warning levels
if (MSVC) 
 list(APPEND TC_WARNING_FLAGS /W3)
else()
  # Set optimization level to "debug"
  #list(APPEND TC_WARNING_FLAGS -Og )
  list(APPEND TC_WARNING_FLAGS -Wall -Wextra -pedantic -Wnon-virtual-dtor -Woverloaded-virtual)
  list(APPEND TC_WARNING_FLAGS -Wconversion)
  list(APPEND TC_WARNING_FLAGS -w)
endif()

message(STATUS "TESTS WARNINGS ${TC_WARNING_FLAGS}")


function( DEFINE_TEST test_name link_lib first_source_file)
  set(test_source_list ${first_source_file} ${ARGN})
  source_group("src" FILES ${test_source_list})
  ADD_EXECUTABLE( ${test_name} "${test_source_list}")
  # SET_TESTS_PROPERTIES( ${test_name} PROPERTIES ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.profraw")
  # As per CMake docs: Add empty generator expr to avoid a configuration subdirectory on multi configs
  set_target_properties(${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ABS_WORKING_DIR}$<0:...>)
  set_target_properties(${test_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ABS_WORKING_DIR})
  set_target_properties(${test_name} PROPERTIES OUTPUT_NAME "${test_name}-${BUILD_BINARY_SUFFIX}")
  target_compile_options(${test_name} PRIVATE ${TC_WARNING_FLAGS})
  if (${link_lib})
    target_link_libraries(${test_name} PUBLIC dawlib)
  else()
    set_target_properties(${test_name} PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
    CONFIGURE_TARGET_DEPS(${test_name})
    # hacky: copy interface libs from dawlib, but don't acutally use dawlib (or all of the libs)
    get_property(DAWLIBTARGETLIBS TARGET dawlib PROPERTY INTERFACE_LINK_LIBRARIES)
    # message(STATUS "COPY PASTA INTERFACE_LINK_LIBRARIES ${DAWLIBTARGETLIBS}")
    target_link_libraries(${test_name} PRIVATE ${DAWLIBTARGETLIBS})
    if (WIN32 AND PROJECT_CFG_USE_STACK_PROTECTOR)
      target_link_libraries(${test_name} PRIVATE ssp)
    endif ()
  endif()
  add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${ABS_WORKING_DIR})
endfunction()

DEFINE_TEST(
  test_seh_exception_handling FALSE 
  test_seh_exception_handling.cpp
)
if (NOT MSVC)
  target_compile_options(test_seh_exception_handling PRIVATE "-fno-lto;-mtune=generic;-fno-function-sections;-fno-data-sections")
endif()

DEFINE_TEST(
  test_math FALSE 
  test_math.cpp
  common/test_common.cpp 
  common/test_stubs.cpp
  ../src/util/testing_environment.cpp
  ../src/util/str_util.cpp
  ../src/util/dsp_util.cpp
  ../src/util/fp_math.cpp
  ../src/threads/thread.cpp
)
set_source_files_properties(../src/util/fp_math.cpp PROPERTIES
  SKIP_UNITY_BUILD_INCLUSION ON
  SKIP_PRECOMPILE_HEADERS ON
)
if (NOT MSVC)
  set_source_files_properties(../src/util/fp_math.cpp PROPERTIES
    COMPILE_OPTIONS "-fno-lto;-mtune=generic;-fno-function-sections;-fno-data-sections;-fno-fast-math;-O2"
  )
endif()

DEFINE_TEST(
  test_strings FALSE
  test_strings.cpp
  common/test_common.cpp
  common/test_stubs.cpp
  ../src/util/testing_environment.cpp
  ../src/util/str_util.cpp
  ../src/threads/thread.cpp
)

DEFINE_TEST(
  test_logger FALSE
  test_logger.cpp
  common/test_common.cpp
  ../src/util/testing_environment.cpp
  ../src/util/logging.cpp
  ../src/util/str_util.cpp
  ../src/threads/thread.cpp
  ../src/platform/${PROJECT_PLATFORM}/fileio.cpp
  ../src/platform/${PROJECT_PLATFORM}/platform_${PROJECT_PLATFORM}.cpp
)
set_source_files_properties("../src/platform/${PROJECT_PLATFORM}/platform_${PROJECT_PLATFORM}.cpp" PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

DEFINE_TEST(test_fileio TRUE test_fileio.cpp common/test_common.cpp)
DEFINE_TEST(test_midifile TRUE test_midifile.cpp common/test_common.cpp)
DEFINE_TEST(test_audioblock TRUE test_audioblock.cpp common/test_common.cpp)
DEFINE_TEST(test_resampler TRUE test_resampler.cpp common/test_common.cpp)
DEFINE_TEST(test_notes TRUE test_notes.cpp common/test_common.cpp)
DEFINE_TEST(test_seqtime TRUE test_seqtime.cpp common/test_common.cpp)
DEFINE_TEST(test_threads TRUE test_threads.cpp common/test_common.cpp)
DEFINE_TEST(test_host TRUE test_host.cpp common/test_common.cpp)
DEFINE_TEST(test_host_minimal TRUE test_host_minimal.cpp common/test_common.cpp)
DEFINE_TEST(test_memory_leak TRUE test_memory_leak.cpp)
DEFINE_TEST(test_projectfile TRUE test_projectfile.cpp)
DEFINE_TEST(test_muparser TRUE test_muparser.cpp)
DEFINE_TEST(test_libarchive TRUE test_libarchive.cpp)
DEFINE_TEST(test_clap_plugin_host TRUE test_clap_plugin_host.cpp)

if (PROJECT_BUILD_TESTS_UI) 
  DEFINE_TEST(test_vstplugins TRUE test_vstplugins.cpp common/test_common.cpp)
  DEFINE_TEST(test_host_exception_handling TRUE test_host_exception_handling.cpp common/test_common.cpp)
  DEFINE_TEST(test_host_project TRUE test_host_project.cpp common/test_common.cpp)
  DEFINE_TEST(test_waveformrenderer TRUE test_waveformrenderer.cpp)
  DEFINE_TEST(test_fontrenderer TRUE test_fontrenderer.cpp)
endif()