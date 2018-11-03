
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
    ${DAW_DEPS_PATH}/portaudio/src/common
    ${DAW_DEPS_PATH}/cereal
    ${DAW_DEPS_PATH}/nvwa/nvwa)


if (WIN32)
    include_directories(${DAW_DEPS_PATH}/ASIOSDK2.3/common
        ${DAW_DEPS_PATH}/ASIOSDK2.3/host
        ${DAW_DEPS_PATH}/ASIOSDK2.3/host/pc
        ${DAW_DEPS_PATH}/portaudio/src/os/win
        ${DAW_DEPS_PATH}/portaudio/src/hostapi/asio
        ${DAW_DEPS_PATH}/portaudio/src/hostapi/dsound
        ${DAW_DEPS_PATH}/portaudio/src/hostapi/wasapi
        ${DAW_DEPS_PATH}/portaudio/src/hostapi/wdmks)
endif()
if (UNIX)
  include_directories(
      ${DAW_DEPS_PATH}/portaudio/src/os/unix
      ${DAW_DEPS_PATH}/portaudio/src/hostapi/alsa)
endif(UNIX)