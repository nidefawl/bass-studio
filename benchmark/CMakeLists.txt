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
    set_target_properties(benchmark-${target_name} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
    set_target_properties(benchmark-${target_name} PROPERTIES EXCLUDE_FROM_ALL ON)
    ADD_POST_BUILD_COMMANDS(benchmark-${target_name})
    SET_APP_BUILD(benchmark-${target_name})
ENDMACRO()

if (NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE MATCHES "Release")
    DEFINE_BENCHMARK(audio-processing)
endif()

