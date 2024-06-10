#include "synth-gpu-snapshot.hpp"

namespace PluginSynth::GPU {

std::shared_ptr<std::vector<std::byte>> serializeSnapshot(const snapshot_t& snapshot) {
    dbgassert(snapshot.version == SYNTH_GPU_SNAPSHOT_VERSION);
    auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
    shrdHeapVec->resize(256);
    DAW::ByteBuffer::stream_write<std::vector<std::byte>> out{*shrdHeapVec, 0};
    out.write(size_t(0));
    out.write(snapshot.version);
    out.write(size_t{snapshot.params.size()});
    out.write(size_t{snapshot.modulations.size()});
    out.write(size_t{snapshot.uiLayout.size()});
    out.write(size_t{snapshot.lfos.size()});
    out.write(size_t{snapshot.adsrs.size()});

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
    for (const auto& uiLayout : snapshot.uiLayout) {
        out.write(uiLayout.uiId);
    }
    for (const auto& lfo : snapshot.lfos) {
        out.write(int32_t{1});
        DAW::Shape::writeShape(out, lfo.shape);
        out.write(lfo.modeIsShape);
        out.write(lfo.randomModeId);
        out.write(lfo.syncFlags);
    }
    for (const auto& adsr : snapshot.adsrs) {
        out.write(int32_t{1});
        out.write(adsr.shapingMode);
    }
    out.setPos(0);
    out.write(size_t(shrdHeapVec->size()));
    return shrdHeapVec;
}
bool deserializeSnapshot(const std::shared_ptr<std::vector<std::byte>>& data, snapshot_t& snapshotOut) {
    if (!data)
        return false;
    DAW::ByteBuffer::stream_read in(*data);
    snapshot_t snapshot;
    size_t dataSize = data->size();
    size_t dataSizeHdr = 0;
    if (!in.read(dataSizeHdr))
        return false;
    if (dataSizeHdr > dataSize)
        return false;
    in.read(snapshot.version);
    if (snapshot.version > SYNTH_GPU_SNAPSHOT_VERSION)
        return false;
    size_t numParams = 0;
    size_t numModulations = 0;
    size_t numUiLayouts = 0;
    size_t numLfos = 0;
    size_t numAdsrs = 0;
    if (!in.read(numParams) || numParams > 1000)
        return false;
    if (!in.read(numModulations) || numModulations > 1000)
        return false;
    if (!in.read(numUiLayouts) || numUiLayouts > 1000)
        return false;
    if (!in.read(numLfos) || numLfos > 1000)
        return false;
    if (!in.read(numAdsrs) || numAdsrs > 1000)
        return false;
    snapshot.params.resize(numParams);
    snapshot.uiLayout.resize(numUiLayouts);
    snapshot.modulations.resize(numModulations);
    snapshot.lfos.resize(numLfos);
    snapshot.adsrs.resize(numAdsrs);

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
            if (!in.read(input.typeIdx))
                return false;
            if (!in.read(input.srcIdx))
                return false;
            if (!in.read(input.opIdx))
                return false;
            if (!in.read(input.value))
                return false;
            if (!in.read(input.range))
                return false;
            if (!in.readString(input.function))
                return false;
        }
        for (auto& dest : modulation.destinations) {
            if (!in.read(dest.paramIdx))
                return false;
            if (!in.read(dest.range))
                return false;
        }
    }
    for (auto& layout : snapshot.uiLayout) {
        if (!in.read(layout.uiId))
            return false;
    }

    for (auto& lfo : snapshot.lfos) {
        int32_t version = 0;
        if (!in.read(version))
            return false;
        if (version < 1)
            return false;
        if (!DAW::Shape::readShape(in, lfo.shape)) {
            return false;
        }
        if (!in.read(lfo.modeIsShape))
            return false;
        if (!in.read(lfo.randomModeId))
            return false;
        if (!in.read(lfo.syncFlags))
            return false;
    }

    for (auto& adsr : snapshot.adsrs) {
        int32_t version = 0;
        if (!in.read(version))
            return false;
        if (version < 1)
            return false;
        if (!in.read(adsr.shapingMode))
            return false;
    }
    snapshotOut = std::move(snapshot);
    return true;
}
} // namespace PluginSynth::GPU
