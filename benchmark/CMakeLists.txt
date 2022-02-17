project("benchmark" C CXX)
find_package(benchmark
    NO_DEFAULT_PATH
    REQUIRED
    PATHS ${PROJECT_DEPS_INSTALL_PATH}/google-benchmark
)

MACRO(DEFINE_BENCHMARK target_name)
    add_executable(${target_name} ${target_name}.cpp)
    target_link_libraries(${target_name} dawlib benchmark::benchmark)
    target_compile_options(${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
    set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
    set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL ON)
    ADD_POST_BUILD_COMMANDS(${target_name})
    SET_APP_BUILD(${target_name})
ENDMACRO()

if (NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE MATCHES "Release")
    DEFINE_BENCHMARK(benchmark-processing)
    DEFINE_BENCHMARK(benchmark-denormals)
endif()

