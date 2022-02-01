#pragma once
#include <cstdint>

struct MidiIOEvent {
    int32_t message;
    int32_t timestamp;
};
