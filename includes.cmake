
include_directories("${DAW_SRC_PATH}")
include_directories("${DAW_SRC_PATH}/include")
include_directories(SYSTEM "${DAW_SRC_PATH}/nanovg")
include_directories(SYSTEM
    ${DAW_DEPS_PATH}/build/lib/glfw/include
    ${DAW_DEPS_PATH}/build/lib/SQLiteCpp/include
    ${DAW_DEPS_PATH}/build/lib/soxr/include
    ${DAW_DEPS_PATH}/glad/include
    ${DAW_DEPS_PATH}/glad/src
    ${DAW_DEPS_PATH}/glm
    ${DAW_DEPS_PATH}/portaudio/include
    ${DAW_DEPS_PATH}/cereal
    ${DAW_DEPS_PATH}/nvwa/nvwa)
