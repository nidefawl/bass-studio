#pragma once

#include <cstdint>
#include <vector>

#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>

class lv2_runtime;

/** Builds one block-sized LV2 atom:MIDI input sequence for a plugin event port. */
class lv2_atom_sequence_writer {
public:
    void configure(lv2_runtime& runtime, uint32_t capacityBytes);
    void begin_block(uint32_t numSamples);
    /** Host transport position at the start of the block (plugins such as Cardinal). */
    void append_time_position(uint32_t frame, int64_t hostFrame, double speed);
    void append_midi(uint32_t frame, uint8_t status, uint8_t data1, uint8_t data2);
    LV2_Atom* finish_block();
    const LV2_Atom* atom() const;
    uint32_t capacity() const { return static_cast<uint32_t>(storage_.size()); }

private:
    lv2_runtime* runtime_{ nullptr };
    std::vector<uint8_t> storage_;
    LV2_Atom_Forge forge_{};
    LV2_Atom_Forge_Frame seq_frame_{};
    uint32_t urid_midi_MidiEvent_{ 0 };
    uint32_t urid_time_Position_{ 0 };
    uint32_t urid_time_frame_{ 0 };
    uint32_t urid_time_speed_{ 0 };
    bool block_open_{ false };
};
