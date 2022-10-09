#pragma once
#include <vector>
#include "byte-buffer.h"
#include "shape.h"
#include "str_util.h"
#include "fileio.h"

namespace DAW::Shape {

struct shape_preset_t {
    int32_t version = 0;
    shape_base_t curve;
};
struct shape_snapshot_t {
    int32_t type = -1;
    shape_preset_t shape;
};
bool saveShapePresetFile(const shape_preset_t& _preset, const String& path);
bool loadShapePresetFile(const String& path, shape_preset_t& preset);

void writeShape(ByteBuffer::stream_write<std::vector<std::byte>>& out, const shape_snapshot_t& shape);
bool readShape(ByteBuffer::stream_read& in, shape_snapshot_t& out);

extern std::vector<SupportedFileType> vFILE_TYPE_SHAPEPRESET;

} // namespace DAW::Shape
