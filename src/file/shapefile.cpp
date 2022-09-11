#include "shape.h"
#include "exceptions.h"
#include "shapefile.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

using cereal::make_nvp;

namespace DAW::Shape {

template <class Archive>
void serialize(Archive& archive, shape_pt_t& m) {
    archive(make_nvp("x", m.pos.x), make_nvp("y", m.pos.y), make_nvp("s", m.shape));
}

template <class Archive>
void serialize(Archive& archive, shape_base_t& m) {
    archive(make_nvp("pts", m.pts));
}

template <class Archive>
void serialize(Archive& archive, shape_preset_t& m) {
    archive(make_nvp("version", m.version));
    archive(
        make_nvp("name", m.name),
        make_nvp("curve", m.curve)
    );
}
bool loadShapePresetFile(const String& path, shape_preset_t& preset) {
    Stringstream ss;
    try {
        std::ifstream file(path, std::ifstream::in);
        if (file) {
            ss << file.rdbuf();
            std::streampos length = file.tellg();
            if (length > 10) {
                shape_preset_t tmp;
                cereal::JSONInputArchive ar(ss);
                ar(tmp);
                preset = tmp;
                return true;
            }
        }
    } catch (const FileIOException& e) {
        log_printf("loadShapePresetFile File IO exception: %s: %s (%d)\n", e.what(), StringAsCStr(path), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("loadShapePresetFile exception: %s\n", e.what());
    }
    return false;
}
bool saveShapePresetFile(const shape_preset_t& _preset, const String& path) {
    try {
        std::ofstream file;
        file.exceptions(~std::ofstream::goodbit);
        file.open(path, std::ofstream::out);
        cereal::JSONOutputArchive ar(file);
        ar(_preset);
        return true;
    } catch (const FileIOException& e) {
        log_printf("savePluginSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("savePluginSnapshot exception: %s\n", e.what());
    }
    return false;
}
const SupportedFileType FILE_TYPE_SHAPEPRESET{ "Shape Preset File", "shape" };
std::vector<SupportedFileType> vFILE_TYPE_SHAPEPRESET = { FILE_TYPE_SHAPEPRESET };

} // namespace DAW::Shape
