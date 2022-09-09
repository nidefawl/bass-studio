#pragma once
#include <vector>
#include "shape.h"
#include "str_util.h"
#include "fileio.h"

namespace DAW::Shape {

struct shape_preset_t {
    int32_t version = 0;
    String name;
    DAW::Shape::shape_base_t curve;
};
bool saveShapePresetFile(const shape_preset_t& _preset, const String& path);
bool loadShapePresetFile(const String& path, shape_preset_t& preset);

extern std::vector<SupportedFileType> vFILE_TYPE_SHAPEPRESET;

} // namespace DAW::Shape
