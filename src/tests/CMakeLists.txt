CONFIGURE_FILE( TestBase.hpp.in ${CMAKE_SOURCE_DIR}/src/tests/TestBase.hpp )

INCLUDE( CheckCXXCompilerFlag )

set(TC_WARNING_FLAGS "")

add_compile_definitions("PROJECT_UNITTEST")


IF( CLANG )
  # Ignore warnings related to the GNU statement expressions if we
  # compile with clang.
  # ENABLE_IF_SUPPORTED("-Wno-gnu-statement-expression" )
ENDIF()

# Enable high warning levels
IF (MSVC) 
 list(APPEND TC_WARNING_FLAGS /W3)
ELSE()

  # Set optimization level to "debug"
  #list(APPEND TC_WARNING_FLAGS -Og )
  list(APPEND TC_WARNING_FLAGS -Wall -Wextra -pedantic -Wnon-virtual-dtor -Woverloaded-virtual)
  list(APPEND TC_WARNING_FLAGS -Wconversion)
  list(APPEND TC_WARNING_FLAGS -w)
ENDIF()

message(STATUS "TESTS WARNINGS ${TC_WARNING_FLAGS}")


function( DEFINE_TEST test_name link_lib first_source_file)
  set(test_source_list ${first_source_file} ${ARGN})
  source_group("src" FILES ${test_source_list})
  ADD_EXECUTABLE( ${test_name} "${test_source_list}")
  get_filename_component(ABS_WORKING_DIR "${PROJECT_WORKING_DIR}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}") # CMAKE_SOURCE_DIR for out of source builds?!
  # SET_TESTS_PROPERTIES( ${test_name} PROPERTIES ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.profraw")
  # As per CMake docs: Add empty generator expr to avoid a configuration subdirectory on multi configs
  set_target_properties(${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ABS_WORKING_DIR}$<0:...>)
  set_target_properties(${test_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ABS_WORKING_DIR})
  set_target_properties(${test_name} PROPERTIES OUTPUT_NAME "${test_name}-${BUILD_BINARY_SUFFIX}")
  set_target_properties(${test_name} PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
  target_compile_options(${test_name} PRIVATE ${TC_WARNING_FLAGS})
  if (${link_lib})
    target_link_libraries(${test_name} PUBLIC dawlib)
  else()
    SET_TARGET_PROJECT_INCLUDE_DIRS(${test_name})
    get_property(libs TARGET dawlib PROPERTY INTERFACE_LINK_LIBRARIES)
    target_link_libraries(${test_name} PRIVATE ${libs})
    if (WIN32 AND PROJECT_CFG_USE_STACK_PROTECTOR)
      target_link_libraries(${test_name} PRIVATE ssp)
    endif ()
  endif()
  add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${ABS_WORKING_DIR})
  ADD_POST_BUILD_COMMANDS(${test_name})
  # get_property(TC_FLAGS TARGET "${test_name}" PROPERTY COMPILE_OPTIONS)
  # message(STATUS "TC TARGET ${test_name} COMPILE_OPTIONS ${TC_FLAGS}")
endfunction()



DEFINE_TEST(test_math FALSE test_math.cpp common/test_common.cpp common/test_environment.cpp common/test_stubs.cpp ../util/str_util.cpp ../threads/thread.cpp)
DEFINE_TEST(test_logger FALSE test_logger.cpp common/test_common.cpp common/test_environment.cpp
        ../util/logging.cpp
        ../util/str_util.cpp
        ../threads/thread.cpp
        ../platform/${PROJECT_PLATFORM}/fileio.cpp
        ../platform/${PROJECT_PLATFORM}/platform_${PROJECT_PLATFORM}.cpp)
DEFINE_TEST(test_audioblock TRUE test_audioblock.cpp common/test_common.cpp)
DEFINE_TEST(test_resampler TRUE test_resampler.cpp common/test_common.cpp)
DEFINE_TEST(test_notes TRUE test_notes.cpp common/test_common.cpp)
DEFINE_TEST(test_seqtime TRUE test_seqtime.cpp common/test_common.cpp)
DEFINE_TEST(test_threads TRUE test_threads.cpp common/test_common.cpp)
DEFINE_TEST(test_vstplugins TRUE test_vstplugins.cpp common/test_common.cpp)
DEFINE_TEST(test_host_minimal TRUE test_host_minimal.cpp common/test_common.cpp)
DEFINE_TEST(test_host_exception_handling TRUE test_host_exception_handling.cpp common/test_common.cpp)
DEFINE_TEST(test_host_project TRUE test_host_project.cpp common/test_common.cpp)
DEFINE_TEST(test_waveformrenderer TRUE test_waveformrenderer.cpp)
