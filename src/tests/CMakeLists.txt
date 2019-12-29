CONFIGURE_FILE( TestBase.hpp.in ${CMAKE_SOURCE_DIR}/src/tests/TestBase.hpp )

INCLUDE( CheckCXXCompilerFlag )

MACRO( ENABLE_IF_SUPPORTED variable flag )
  STRING( REGEX REPLACE "^-" "" flagName "${flag}" )
  CHECK_CXX_COMPILER_FLAG( "${flag}"
    COMPILER_HAS_FLAG_${flagName}
    )
  IF( COMPILER_HAS_FLAG_${flagName} )
    SET( ${variable} "${${variable}} ${flag}")
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

MACRO( DEFINE_TEST test_name )
  ADD_EXECUTABLE( ${test_name} "${test_name}.cpp" test_common.cpp)
  target_link_libraries(${test_name} PRIVATE "dawlib")
  ADD_TEST(NAME ${test_name} COMMAND ${test_name} WORKING_DIRECTORY ${DAW_WORKING_DIR})
  SET_TESTS_PROPERTIES( ${test_name} PROPERTIES ENVIRONMENT "PATH=${DAW_WORKING_DIR};$ENV{PATH}" )
ENDMACRO()

DEFINE_TEST(test_audioblock)
DEFINE_TEST(test_notes)
DEFINE_TEST(test_seqtime)
DEFINE_TEST(test_threads)
DEFINE_TEST(test_waveformrenderer)
DEFINE_TEST(test_vstplugins)
