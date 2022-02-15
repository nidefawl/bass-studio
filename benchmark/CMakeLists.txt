project("benchmark" C CXX) 

MACRO(DEFINE_BENCHMARK target_name)
    find_package(benchmark REQUIRED) 
    add_executable(benchmark-${target_name} ${target_name}.cpp)
    target_link_libraries(benchmark-${target_name} PRIVATE dawlib benchmark::benchmark)
    target_compile_options(benchmark-${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
    ADD_POST_BUILD_COMMANDS(benchmark-${target_name})
    SET_APP_BUILD(benchmark-${target_name})
ENDMACRO()

DEFINE_BENCHMARK(audio-processing)

set(CMAKE_VERBOSE_MAKEFILE ON)

