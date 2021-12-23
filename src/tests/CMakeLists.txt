CONFIGURE_FILE( TestBase.hpp.in ${CMAKE_SOURCE_DIR}/src/tests/TestBase.hpp )

INCLUDE( CheckCXXCompilerFlag )

set(TC_WARNING_FLAGS "")


add_compile_definitions("DAW_UNITTEST")


IF( CLANG )
  # Ignore warnings related to the GNU statement expressions if we
  # compile with clang.
  # ENABLE_IF_SUPPORTED("-Wno-gnu-statement-expression" )
ENDIF()

# Enable high warning levels
IF (MSVC) 
  list(APPEND TC_WARNING_FLAGS /W3 /Wall)
ELSE()

  # Set optimization level to "debug"
  #list(APPEND TC_WARNING_FLAGS -Og )
  list(APPEND TC_WARNING_FLAGS -Wall -Wextra -pedantic -Wnon-virtual-dtor -Woverloaded-virtual)
  list(APPEND TC_WARNING_FLAGS -Wconversion)
ENDIF()

message(STATUS "TESTS WARNINGS ${TC_WARNING_FLAGS}")


function( CONFIGURE_AS_TEST test_name)
  get_filename_component(ABS_WORKING_DIR "${APP_WORKING_DIR}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}") # CMAKE_SOURCE_DIR for out of source builds?!
  # SET_TESTS_PROPERTIES( ${test_name} PROPERTIES ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.profraw")
  set_target_properties(${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ABS_WORKING_DIR}$<$<CONFIG:Debug>:>)
  set_target_properties(${test_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ABS_WORKING_DIR})
  target_compile_options(${test_name} PRIVATE ${TC_WARNING_FLAGS})
  target_link_libraries(${test_name} PRIVATE "dawlib")
  add_test(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${ABS_WORKING_DIR})
  ADD_POST_BUILD_COMMANDS(${test_name})
  # get_property(TC_FLAGS TARGET "${test_name}" PROPERTY COMPILE_OPTIONS)
  # message(STATUS "TC TARGET ${test_name} COMPILE_OPTIONS ${TC_FLAGS}")
endfunction()
function( DEFINE_TEST test_name )
  ADD_EXECUTABLE( ${test_name} "${test_name}.cpp" test_common.cpp)
  CONFIGURE_AS_TEST(${test_name})
endfunction()

DEFINE_TEST(test_logger)
ADD_EXECUTABLE(test_waveformrenderer test_waveformrenderer.cpp)
CONFIGURE_AS_TEST(test_waveformrenderer)
DEFINE_TEST(test_audioblock)
DEFINE_TEST(test_notes)
DEFINE_TEST(test_seqtime)
DEFINE_TEST(test_threads)
DEFINE_TEST(test_vstplugins)
DEFINE_TEST(test_host_minimal)
DEFINE_TEST(test_host_exception_handling)
DEFINE_TEST(test_host_project)
