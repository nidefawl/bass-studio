CONFIGURE_FILE( TestBase.hpp.in ${CMAKE_SOURCE_DIR}/src/tests/TestBase.hpp )

INCLUDE( CheckCXXCompilerFlag )

MACRO( ENABLE_IF_SUPPORTED variable flag )
  STRING( REGEX REPLACE "^-" "" flagName "${flag}" )
  CHECK_CXX_COMPILER_FLAG( "${flag}"
    COMPILER_HAS_FLAG_${flagName}
    )
  IF( COMPILER_HAS_FLAG_${flagName} )
    SET( ${variable} "${${variable}} ${flag}")
    message(STATUS "Compiler has ${flag}: yes")
  ELSE()
    message(STATUS "Compiler has ${flag}: no")
  ENDIF()
ENDMACRO()


IF( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  # Ignore warnings related to the GNU statement expressions if we
  # compile with clang.
  ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Wno-gnu-statement-expression" )
ENDIF()

# Optimizes for a useful debugging experience. While the test cases
# might run slightly faster if "real" optimizations were enabled, I
# like this flag for debugging.
#ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Og" )
ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-g" )

#
# The tests should always have these nasty flags enabled. What's the
# use of the test without them?
#
# Coincidentally, isn't it nice how "-Wall" will not enable any of
# the flags described afterwards?
#
ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Wall" )
ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Wextra" )

# ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Werror" )

ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Wconversion" )
ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Wnon-virtual-dtor" )
#ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Wold-style-cast" )
ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Woverloaded-virtual" )
ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Wself-init" )
ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-Wunsafe-loop-optimization" )

ENABLE_IF_SUPPORTED( CMAKE_CXX_FLAGS "-pedantic" )
add_compile_definitions("DAW_UNITTEST")


MACRO( CONFIGURE_AS_TEST test_name)
  target_link_libraries(${test_name} PRIVATE "dawlib")
  get_filename_component(ABS_WORKING_DIR "${APP_WORKING_DIR}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}") # CMAKE_SOURCE_DIR for out of source builds?!
  ADD_TEST(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${ABS_WORKING_DIR})
  # SET_TESTS_PROPERTIES( ${test_name} PROPERTIES ENVIRONMENT "LLVM_PROFILE_FILE=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.profraw")
  set_target_properties(${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${ABS_WORKING_DIR}$<$<CONFIG:Debug>:>)
  set_target_properties(${test_name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ABS_WORKING_DIR})
  ADD_POST_BUILD_COMMANDS(${test_name})
ENDMACRO()
MACRO( DEFINE_TEST test_name )
  ADD_EXECUTABLE( ${test_name} "${test_name}.cpp" test_common.cpp)
  CONFIGURE_AS_TEST(${test_name})
ENDMACRO()

DEFINE_TEST(test_audioblock)
DEFINE_TEST(test_notes)
DEFINE_TEST(test_seqtime)
DEFINE_TEST(test_threads)
DEFINE_TEST(test_vstplugins)
DEFINE_TEST(test_host_minimal)
DEFINE_TEST(test_host_exception_handling)
DEFINE_TEST(test_host_project)
ADD_EXECUTABLE(test_waveformrenderer test_waveformrenderer.cpp)
CONFIGURE_AS_TEST(test_waveformrenderer)
