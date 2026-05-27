#include "host/plugin/lv2/lv2-atoms.hpp"

#include "host/plugin/lv2/lv2-runtime.hpp"

#include <lv2/atom/atom.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>

void lv2_atom_sequence_writer::configure(lv2_runtime& runtime, uint32_t capacityBytes) {
    runtime_             = &runtime;
    urid_midi_MidiEvent_ = runtime.urid(LV2_MIDI__MidiEvent);
    urid_time_Position_  = runtime.urid(LV2_TIME__Position);
    urid_time_frame_     = runtime.urid(LV2_TIME__frame);
    urid_time_speed_     = runtime.urid(LV2_TIME__speed);
    storage_.assign(capacityBytes > 64 ? capacityBytes : 4096, 0);
    lv2_atom_forge_init(&forge_, runtime.urid_map());
}

void lv2_atom_sequence_writer::begin_block(uint32_t numSamples) {
    if (!runtime_ || storage_.empty()) {
        return;
    }
    lv2_atom_forge_set_buffer(&forge_, storage_.data(), static_cast<uint32_t>(storage_.size()));
    (void)numSamples;
    lv2_atom_forge_sequence_head(&forge_, &seq_frame_, 0);
    block_open_ = true;
}

void lv2_atom_sequence_writer::append_time_position(uint32_t frame, int64_t hostFrame, double speed) {
    if (!block_open_ || !runtime_) {
        return;
    }
    LV2_Atom_Forge_Frame obj_frame;
    lv2_atom_forge_frame_time(&forge_, frame);
    lv2_atom_forge_object(&forge_, &obj_frame, 0, urid_time_Position_);
    lv2_atom_forge_key(&forge_, urid_time_frame_);
    lv2_atom_forge_long(&forge_, hostFrame);
    lv2_atom_forge_key(&forge_, urid_time_speed_);
    lv2_atom_forge_float(&forge_, static_cast<float>(speed));
    lv2_atom_forge_pop(&forge_, &obj_frame);
}

void lv2_atom_sequence_writer::append_midi(uint32_t frame, uint8_t status, uint8_t data1, uint8_t data2) {
    if (!block_open_) {
        return;
    }
    LV2_Atom_Forge_Frame ev_frame;
    lv2_atom_forge_frame_time(&forge_, frame);
    lv2_atom_forge_atom(&forge_, 3, urid_midi_MidiEvent_);
    const uint8_t msg[3] = { status, data1, data2 };
    lv2_atom_forge_write(&forge_, msg, sizeof(msg));
    lv2_atom_forge_pop(&forge_, &ev_frame);
}

LV2_Atom* lv2_atom_sequence_writer::finish_block() {
    if (block_open_) {
        lv2_atom_forge_pop(&forge_, &seq_frame_);
        block_open_ = false;
    }
    return storage_.empty() ? nullptr : reinterpret_cast<LV2_Atom*>(storage_.data());
}

const LV2_Atom* lv2_atom_sequence_writer::atom() const {
    return storage_.empty() ? nullptr : reinterpret_cast<const LV2_Atom*>(storage_.data());
}
