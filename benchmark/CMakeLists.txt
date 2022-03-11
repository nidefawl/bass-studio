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
      
    target_link_libraries(${target_name} benchmark::benchmark)
    SET_TARGET_PROJECT_INCLUDE_DIRS(${target_name})
    # hacky: copy interface libs from dawlib, but don't acutally use dawlib (or all of the libs)
    get_property(DAWLIBTARGETLIBS TARGET dawlib PROPERTY INTERFACE_LINK_LIBRARIES)
    # message(STATUS "COPY PASTA INTERFACE_LINK_LIBRARIES ${DAWLIBTARGETLIBS}")
    target_link_libraries(${target_name} ${DAWLIBTARGETLIBS})
    target_compile_options(${target_name} PRIVATE ${BUILD_WARNING_FLAGS})
    set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
    set_target_properties(${target_name} PROPERTIES EXCLUDE_FROM_ALL ON)
    ADD_POST_BUILD_COMMANDS(${target_name})
    SET_APP_BUILD(${target_name})
ENDMACRO()

DEFINE_BENCHMARK(benchmark-processing)
DEFINE_BENCHMARK(benchmark-denormals)
DEFINE_BENCHMARK(benchmark-timer)
DEFINE_MINIMAL_BENCHMARK(benchmark-meters)
