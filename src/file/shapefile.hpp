#pragma once
#include <vector>
#include "byte-buffer.hpp"
#include "host/shape/shape.hpp"
#include "str_util.hpp"
#include "fileio.hpp"
#include <cereal/cereal.hpp>
#include <cereal/types/vector.hpp>

namespace DAW::Shape {

struct shape_preset_t {
    int32_t version = 0;
    shape_t curve;
};
struct shape_snapshot_t {
    int32_t type = -1;
    shape_preset_t shape;
};
bool saveShapePresetFile(const shape_preset_t& _preset, const String& path);
bool loadShapePresetFile(const String& path, shape_preset_t& preset);

void writeShape(ByteBuffer::stream_write<std::vector<std::byte>>& out, const shape_snapshot_t& shape);
bool readShape(ByteBuffer::stream_read& in, shape_snapshot_t& out);

extern const SupportedFileTypes FILE_TYPES_SHAPEPRESET;


template <class Archive>
void serialize(Archive& archive, shape_pt_t& m) {
    archive(
        cereal::make_nvp("x", m.pos.x),
        cereal::make_nvp("y", m.pos.y),
        cereal::make_nvp("s", m.shape)
    );
}

template <class Archive>
void serialize(Archive& archive, shape_t& m) {
    archive(
        cereal::make_nvp("name", m.name),
        cereal::make_nvp("flags", m.flags),
        cereal::make_nvp("pts", m.pts)
    );
}
} // namespace DAW::Shape
