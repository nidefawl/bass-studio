#include "str_util.h"
#include "logging.h"
#include "synth-plugin.h"
#include "synth-snapshot.h"
#include <array>
#include <cstdint>
#include <utility>

namespace PluginSynth {

template<typename T>
struct stream_write {
    T& stream;
    size_t pos;
    template<typename D>
    void write(const D& input) {
        if (stream.size() < pos + sizeof(D)) {
            stream.resize(stream.size() + math::max<size_t>(sizeof(D), 128));
        }
        const auto* pInput = reinterpret_cast<const std::byte*>(&input);
        std::memcpy(stream.data() + pos, pInput, sizeof(D));
        pos += sizeof(D);
    }
    void writeString(const String& str) {
        auto sizeStr = static_cast<int32_t>(str.length());
        write<int32_t>(sizeStr);
        if (stream.size() < pos + sizeStr) {
            stream.resize(stream.size() + math::max<size_t>(sizeStr, 128));
        }
        const auto* pInput = reinterpret_cast<const std::byte*>(str.data());
        std::memcpy(stream.data() + pos, pInput, sizeStr);
        pos += sizeStr;
    }
    void setPos(size_t p) {
        pos = p;
    }
};

std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
    dbgassert(snapshot.version == SYNTH_SNAPSHOT_VERSION);
    auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
    shrdHeapVec->resize(256);
    stream_write<std::vector<std::byte>> out{*shrdHeapVec, 0};
    out.write(size_t(0));
    out.write(snapshot.version);
    out.write(size_t{snapshot.params.size()});
    out.write(size_t{snapshot.modulations.size()});
    out.write(size_t{snapshot.uiLayout.size()});
    out.write(size_t{snapshot.settings.size()});
    out.write(size_t{snapshot.shapes.size()});
    for (const auto& p : snapshot.params) {
        out.write(p.paramIdx);
        out.write(p.value);
    }
    for (const auto& modulation : snapshot.modulations) {
        out.write(modulation.slotIdx);
        out.write(size_t{modulation.inputs.size()});
        out.write(size_t{modulation.destinations.size()});
        for (const auto& input : modulation.inputs) {
            out.write(input.typeIdx);
            out.write(input.srcIdx);
            out.write(input.opIdx);
            out.write(input.value);
            out.write(input.range);
            out.writeString(input.function);
        }
        for (const auto& dest : modulation.destinations) {
            out.write(dest.paramIdx);
            out.write(dest.range);
        }
    }
    for (const auto& modulation : snapshot.uiLayout) {
        out.write(modulation.uiId);
        out.write(modulation.splitPos);
    }
    for (const auto& setting : snapshot.settings) {
        out.writeString(stringsSettings[setting.paramIdx]);
        out.write(setting.range);
    }
    for (const auto& shape : snapshot.shapes) {
        out.write(shape.type);
        out.write(shape.shape.version);
        out.writeString(shape.shape.name);
        out.write(size_t{shape.shape.curve.pts.size()});
        for (const auto& point : shape.shape.curve.pts) {
            out.write(point.pos.x);
            out.write(point.pos.y);
            out.write(point.shape);
        }
    }
    out.setPos(0);
    out.write(size_t(shrdHeapVec->size()));
    return shrdHeapVec;
}
struct stream_read {
    const std::byte* const dataBegin;
    const size_t size;
    const std::byte* data;
    size_t pos;
    template<typename T>
    explicit stream_read(const T& vec) 
        : dataBegin(vec.data()),
        size(vec.size()),
        data(vec.data()),
        pos(0)
    { }
    template<typename T>
    bool read(T& out) {
        if (pos + sizeof(T) > size) return false;
        T tmp{};
        std::memcpy(&tmp, &data[pos], sizeof(T));
        pos += sizeof(T);
        out = tmp;
        return true;
    }
    bool readString(String& out) {
        int32_t stringSize = 0;
        if (!read(stringSize)) return false;
        if (pos + stringSize > size) return false;
        String str;
        str.resize(stringSize);
        std::memcpy(str.data(), &data[pos], stringSize);
        out = std::move(str);
        pos += stringSize;
        return true;
    }
};
bool deserializeSnapshot(const std::shared_ptr<std::vector<std::byte>>& data, snapshot_t& snapshotOut) {
    if (!data)
        return false;
    stream_read in(*data);
    snapshot_t snapshot;
    size_t dataSize = data->size();
    size_t dataSizeHdr = 0;
    if (!in.read(dataSizeHdr))
        return false;
    if (dataSizeHdr > dataSize)
        return false;
    in.read(snapshot.version);
    if (snapshot.version < 2)
        return false;
    if (snapshot.version > SYNTH_SNAPSHOT_VERSION)
        return false;
    size_t numParams = 0;
    size_t numModulations = 0;
    size_t numUiLayouts = 0;
    size_t numSettings = 0;
    size_t numShapes = 0;
    if (!in.read(numParams) || numParams > 1000)
        return false;
    if (!in.read(numModulations) || numModulations > 1000)
        return false;
    if (snapshot.version >= 7) {
        if (!in.read(numUiLayouts) || numUiLayouts > 1000)
            return false;
    }
    if (snapshot.version >= 8) {
        if (!in.read(numSettings) || numSettings > 1000)
            return false;
    }
    if (snapshot.version >= 9) {
        if (!in.read(numShapes) || numShapes > 1000)
            return false;
    }
    snapshot.params.resize(numParams);
    snapshot.modulations.resize(numModulations);
    snapshot.uiLayout.resize(numUiLayouts);

    for (auto& p : snapshot.params) {
        if (!in.read(p.paramIdx))
            return false;
        if (!in.read(p.value))
            return false;
    }
    for (auto& modulation : snapshot.modulations) {
        if (!in.read(modulation.slotIdx))
            return false;
        size_t numInputs = 0;
        size_t numDestinations = 0;
        if (!in.read(numInputs))
            return false;
        if (!in.read(numDestinations))
            return false;
        modulation.inputs.resize(numInputs);
        modulation.destinations.resize(numDestinations);
        for (auto& input : modulation.inputs) {
            if (snapshot.version < 5) {
                int32_t singleIdx = 0;
                if (!in.read(singleIdx))
                    return false;
                input.typeIdx = math::clamp(singleIdx, 0, 2);
                if (input.typeIdx == 0) input.typeIdx = 1;
                else if (input.typeIdx == 1) input.typeIdx = 0;
                input.srcIdx = 0;
                if (singleIdx >= 3)
                    input.srcIdx = math::clamp(singleIdx - 2, 0, 7);
            } else {
                if (!in.read(input.typeIdx))
                    return false;
                if (!in.read(input.srcIdx))
                    return false;
            }
            if (!in.read(input.opIdx))
                return false;
            if (!in.read(input.value))
                return false;
            if (snapshot.version >= 3) {
                if (!in.read(input.range))
                    return false;
            }
            if (snapshot.version >= 4) {
                if (!in.readString(input.function))
                    return false;
            }
        }
        for (auto& dest : modulation.destinations) {
            if (!in.read(dest.paramIdx))
                return false;
            if (!in.read(dest.range))
                return false;
        }
    }
    if (snapshot.version >= 7) {
        for (auto& modulation : snapshot.uiLayout) {
            if (snapshot.version >= 10) {
                if (!in.read(modulation.uiId))
                    return false;
            }
            if (!in.read(modulation.splitPos))
                return false;
        }
    }
    if (snapshot.version >= 8) {
        snapshot.settings.reserve(numSettings);
        for (size_t i = 0; i < numSettings; ++i) {
            String settingType;
            if (!in. readString(settingType))
                return false;
            float value = 0.0;
            if (!in.read(value))
                return false;
            for (size_t j = 0; j < stringsSettings.size(); ++j) {
                if (settingType == stringsSettings[j]) {
                    snapshot.settings.push_back(setting_snapshot_t{ static_cast<int32_t>(j), value });
                    break;
                }
            }
        }
    }
    if (snapshot.version >= 9) {
        snapshot.shapes.reserve(numShapes);
        for (size_t i = 0; i < numShapes; ++i) {
            int32_t shapeType = 0;
            if (!in.read(shapeType))
                return false;
            DAW::Shape::shape_preset_t shape;
            if (!in.read(shape.version))
                return false;
            if (!in. readString(shape.name))
                return false;
            size_t numPoints = 0;
            if (!in.read(numPoints))
                return false;
            if (numPoints > 4096)
                return false;
            for (size_t j = 0; j < numPoints; ++j) {
                float x = 0.0;
                if (!in. read(x))
                    return false;
                float y = 0.0;
                if (!in. read(y))
                    return false;
                float s = 0.0;
                if (!in. read(s))
                    return false;
                shape.curve.pts.push_back({{ x, y }, s});
            }
            snapshot.shapes.push_back(shape_snapshot_t{ shapeType, std::move(shape) });
        }
    }
    snapshotOut = std::move(snapshot);
    return true;
}

} // namespace PluginSynth