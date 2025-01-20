#include "fileio.hpp"
#include "host/shape/shape.hpp"
#include "exceptions.hpp"
#include "shapefile.hpp"

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

struct old_shape_base_t {
    std::vector<shape_pt_t> pts;
    String name;
    float renderPhase = -1.0f;
    int32_t flags;
};

template <class Archive>
void serialize(Archive& archive, old_shape_base_t& m) {
    archive(make_nvp("pts", m.pts));
}

template<class Archive>
void load(Archive& archive, shape_preset_t& m) {
    archive(make_nvp("version", m.version));
    if (m.version < 2) {
        old_shape_base_t old;
        m.curve = {};
        archive(
            make_nvp("name", m.curve.name),
            make_nvp("curve", old)
        );
        m.curve.flags = SHAPE_CYCLIC | SHAPE_SHAPED;
        m.curve.pts = old.pts;
    } else {
        archive(
            make_nvp("name", m.curve.name),
            make_nvp("curve", m.curve)
        );
    }
}

template<class Archive>
void save(Archive& archive, shape_preset_t const& m) {
    archive(
        make_nvp("version", m.version),
        make_nvp("name", m.curve.name),
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

const SupportedFileTypes FILE_TYPES_SHAPEPRESET = SupportedFileTypes{"Shape Preset", { SupportedFileType{ "Shape Preset File", "shape" } } };

} // namespace DAW::Shape
