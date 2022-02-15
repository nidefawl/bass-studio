project("benchmark" C CXX)
find_package(benchmark
    NO_DEFAULT_PATH
    REQUIRED
    PATHS ${PROJECT_DEPS_INSTALL_PATH}/google-benchmark
)

MACRO(DEFINE_BENCHMARK target_name)
    add_executable(benchmark-${target_name} ${target_name}.cpp)
    target_link_libraries(benchmark-${target_name} dawlib benchmark::benchmark)
    target_compile_options(benchmark-${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
    set_target_properties(benchmark-${target_name} PROPERTIES EXCLUDE_FROM_ALL $<IF:$<CONFIG:Release>,false,true>)
    ADD_POST_BUILD_COMMANDS(benchmark-${target_name})
    SET_APP_BUILD(benchmark-${target_name})
ENDMACRO()

DEFINE_BENCHMARK(audio-processing)

set(CMAKE_VERBOSE_MAKEFILE ON)

