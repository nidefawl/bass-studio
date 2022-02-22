project("benchmark" C CXX)

find_package(benchmark PATHS ${PROJECT_DEPS_INSTALL_PATH} REQUIRED NO_DEFAULT_PATH)

MACRO(DEFINE_BENCHMARK target_name)
    add_executable(${target_name} ${target_name}.cpp)
    target_link_libraries(${target_name} dawlib benchmark::benchmark)
    target_compile_options(${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
    set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
    set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL ON)
    ADD_POST_BUILD_COMMANDS(${target_name})
    SET_APP_BUILD(${target_name})
ENDMACRO()

DEFINE_BENCHMARK(benchmark-processing)
DEFINE_BENCHMARK(benchmark-denormals)
DEFINE_BENCHMARK(benchmark-timer)
