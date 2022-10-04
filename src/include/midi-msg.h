#pragma once
#include "types.h"

struct MidiIOEvent {
    uint32_t message;
    int32_t timestamp;
};
