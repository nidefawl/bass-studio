project("benchmark" C CXX)

find_package(benchmark PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)

MACRO(DEFINE_BENCHMARK target_name)
  add_executable(${target_name} ${target_name}.cpp)
  target_link_libraries(${target_name} PRIVATE dawlib benchmark::benchmark)
  target_compile_options(${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
  set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
  set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL ON)
  CONFIGURE_TARGET_OUTPUT(${target_name})
ENDMACRO()

if (NOT DEFINED COMPILER_SUPPORTS_MTUNE_NATIVE)
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-mtune=native" COMPILER_SUPPORTS_MTUNE_NATIVE)
  check_cxx_compiler_flag("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
endif()

MACRO(DEFINE_MINIMAL_BENCHMARK target_name)
  add_executable(${target_name}
    ${target_name}.cpp
    ../src/host/audiobuffer.cpp
    ../test/common/test_common.cpp
    ../src/util/testing_environment.cpp
    ../src/util/logging.cpp
    ../src/util/str_util.cpp
    ../src/threads/thread.cpp
    ../src/platform/${PROJECT_PLATFORM}/fileio.cpp
    ../src/platform/${PROJECT_PLATFORM}/platform_${PROJECT_PLATFORM}.cpp
  )
  set_source_files_properties("../src/platform/${PROJECT_PLATFORM}/platform_${PROJECT_PLATFORM}.cpp" PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    
  target_link_libraries(${target_name} PRIVATE benchmark::benchmark)
  CONFIGURE_TARGET_DEPS(${target_name})
  # hacky: copy interface libs from dawlib, but don't acutally use dawlib (or all of the libs)
  get_property(DAWLIBTARGETLIBS TARGET dawlib PROPERTY INTERFACE_LINK_LIBRARIES)
  # message(STATUS "COPY PASTA INTERFACE_LINK_LIBRARIES ${DAWLIBTARGETLIBS}")
  target_link_libraries(${target_name} PRIVATE ${DAWLIBTARGETLIBS})
  target_compile_options(${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
  set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
  set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL ON)
  CONFIGURE_TARGET_OUTPUT(${target_name})
  if (COMPILER_SUPPORTS_MARCH_NATIVE)
    target_compile_options(${target_name} PUBLIC -march=native)
  endif ()
  if (COMPILER_SUPPORTS_MTUNE_NATIVE)
    target_compile_options(${target_name} PUBLIC -mtune=native)
  endif ()
ENDMACRO()

DEFINE_BENCHMARK(benchmark-processing)
DEFINE_BENCHMARK(benchmark-denormals)
DEFINE_BENCHMARK(benchmark-timer)
DEFINE_MINIMAL_BENCHMARK(benchmark-meters)
DEFINE_BENCHMARK(benchmark-fontrenderer)
DEFINE_BENCHMARK(benchmark-simd)
