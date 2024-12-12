project("rgb-controller" C CXX)
set(RGB_CONTROLLER_TARGET "rgb-controller")
set(RGB_CONTROLLER_SRC_FILES 
  rgb-controller.cpp
  rgb-controller.h
  net/network.cpp
  net/network.h
  net/packet.h
)

add_executable(${RGB_CONTROLLER_TARGET} ${RGB_CONTROLLER_SRC_FILES})
target_link_libraries(${RGB_CONTROLLER_TARGET} PRIVATE dawlib)
target_compile_options(${RGB_CONTROLLER_TARGET} PRIVATE ${BUILD_WARNING_FLAGS})
set_target_properties(${RGB_CONTROLLER_TARGET} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD ON)
set_target_properties(${RGB_CONTROLLER_TARGET} PROPERTIES EXCLUDE_FROM_ALL ON)

CONFIGURE_TARGET_OUTPUT(${RGB_CONTROLLER_TARGET} ${RGB_CONTROLLER_TARGET} ${RGB_CONTROLLER_TARGET})
