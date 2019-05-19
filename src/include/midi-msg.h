#pragma once
#include <stdint.h>

struct MidiIOEvent {
	int32_t message;
	int32_t timestamp;
};
