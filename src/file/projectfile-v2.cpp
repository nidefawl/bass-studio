#include "projectfile-v2.hpp"
#include "projectfile-v1.hpp"
#include "groovefile.hpp"
#include "snapshot/snapshot.hpp"
#include "snapshot/trackrouting-snapshot.hpp"
#include "snapshot/track-snapshot.hpp"
#include "snapshot/plugin-snapshot.hpp"
#include "snapshot/project-snapshot.hpp"
#include "gui/container/container_layout_types.hpp"

#include <exception>
#include <glm/ext/vector_int4.hpp>
#include <vector>
#include <sstream>
#include <algorithm>
#include <memory>
#include <functional>

#include "config.hpp"
#include "platform.hpp"
#include "exceptions.hpp"
#include "seq_time.hpp"
#include "seq_util.hpp"
#include "math/vec.hpp"
#include "str_util.hpp"
#include "fileio.hpp"
#include "logging.hpp"
#include "layout.hpp"
#include "shapefile.hpp"
#include "host/clip/clip.hpp"
#include "host/daw_channel.hpp"
#include "host/automation/automation.hpp"
#include "host/track/track.hpp"
#include "host/project/project.hpp"
#include "host/plugin/modules.hpp"

#include <nlohmann/json.hpp>
#include <cereal/external/base64.hpp>

using json = nlohmann::json;
using namespace nlohmann::literals;
#define JSON_FROM_TO NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT

namespace glm {
JSON_FROM_TO(ivec2, x, y)
JSON_FROM_TO(ivec3, x, y, z)
JSON_FROM_TO(ivec4, x, y, z, w)
JSON_FROM_TO(vec2, x, y)
JSON_FROM_TO(vec3, x, y, z)
JSON_FROM_TO(vec4, x, y, z, w)
} // namespace glm

JSON_FROM_TO(automation_point_t, time, val)
JSON_FROM_TO(automation_view_t, targetParam, points, active)
JSON_FROM_TO(mixerlayout_settings_t, width)
JSON_FROM_TO(mixer_layout_snapshot_t, layout)
JSON_FROM_TO(automatable_param_ref_t, type, paramIdx, refId)
JSON_FROM_TO(subtracksettings_t, subtrackType)
JSON_FROM_TO(param_snapshot_t, idx, val, flags)
JSON_FROM_TO(subtracklayout_settings_t, height)
JSON_FROM_TO(subtrack_snapshot_t, settings, layoutSettings, atlRef)
JSON_FROM_TO(tracklayout_settings_t, height, hideSubtracks, foldTrack)
JSON_FROM_TO(track_layout_snapshot_t, layout, subtracks)
JSON_FROM_TO(track_params_snapshot_t, params, automatedParams)
JSON_FROM_TO(arp_snapshot, params, automatedParams)
JSON_FROM_TO(plugin_ui_snapshot_t, parameterListVisible, layoutMode)

namespace DAW {
JSON_FROM_TO(DAW::modulation_scaling_t, min, max, mode, bClamp)
JSON_FROM_TO(DAW::channel_desc, name, count, offset)
JSON_FROM_TO(DAW::modulation_channel_ref, paramIdxDst, refSrc, scale, bIsTemporary)
JSON_FROM_TO(DAW::Cursor, cursorPos, cursorTrack, cursorSubTrack, selRange, selTrackRange, selSubTrackRange)
} // namespace DAW

JSON_FROM_TO(plugin_iodesc_snapshot_t, input, output)
JSON_FROM_TO(track_effect_routing_snapshot_t, routingState, inputRoutingOutputStage, inputRoutingEffects)
JSON_FROM_TO(io_configuration_snapshot_t, type, stageId, stageEndPointType, externalInputType, projectGlobalId, externalInputIdx, srcChannelOffset, dstChannelOffset)
JSON_FROM_TO(io_midi_snapshot_t, type, stageId, stageEndPointType, srcChannel, dstChannel, inputName)
JSON_FROM_TO(track_io_configuration_snapshot_t, input, output, midiInputs, midiOutput)
JSON_FROM_TO(export_settings_t, exportPos, exportLen, exportPath, isLocked) 
JSON_FROM_TO(quantize_settings, quantizeStart, quantizeEnd)
JSON_FROM_TO(note_t, time, len, pitch, flags, velocity)
JSON_FROM_TO(groove_timing_data_t, timePoints, velocityPoints, loopLength)
JSON_FROM_TO(groove_data_t, timingData, presetName, grooveName, lenQuantization, strengthQuantization, strengthGroove, strengthVelocity, randomTiming, randomVelocity)


inline void to_json(json& j, const appwindow_size_t& m) {
    // convert to base64:
    auto base64string = cereal::base64::encode(reinterpret_cast<const unsigned char *>( m.data ), sizeof(m.data));
    j = json{
        {"data", base64string},
        {"type", m.type},
        {"valid", m.valid}
    };

}
inline void from_json(const json& j, appwindow_size_t& m) {
    j.at("type").get_to(m.type);
    j.at("valid").get_to(m.valid);
    auto base64string = j.at("data").get<std::string>();
    auto decoded = cereal::base64::decode(base64string);
    if (decoded.size() != sizeof(m.data)) {
        throw std::runtime_error("Invalid data size");
    }
    std::memcpy(m.data, decoded.data(), sizeof(m.data));
}
void to_json(json& j, const plugin_windowlayout_snapshot_t& m) {
    j = json{
        {"windowPosSize", m.windowPosSize},
        {"windowPosSizeValid", m.windowPosSizeValid},
        {"isWindowOpen", m.isWindowOpen},
        {"windowSize", m.windowSize}
    };
}

void from_json(const json& j, plugin_windowlayout_snapshot_t& m) {
    j.at("windowPosSize").get_to(m.windowPosSize);
    j.at("windowPosSizeValid").get_to(m.windowPosSizeValid);
    j.at("isWindowOpen").get_to(m.isWindowOpen);
    j.at("windowSize").get_to(m.windowSize);
    m.isValidSnapshot = true;
}

void to_json(json& j, const track_id_snapshot_t& m) {
    j = json{
        {"stageid", m.stageId},
        {"input_stageid", m.inputStageId},
        {"output_stageid", m.outputStageId},
        {"outputpost_stageid", m.outputPostStageId}
    };
}

void from_json(const json& j, track_id_snapshot_t& m) {
    j.at("stageid").get_to(m.stageId);
    j.at("input_stageid").get_to(m.inputStageId);
    j.at("output_stageid").get_to(m.outputStageId);
    j.at("outputpost_stageid").get_to(m.outputPostStageId);
}

void to_json(json& j, const track_modulation_routing_snapshot_t& m) {
    j = json{
        {"modulations", m.effectMods},
        {"mixer", m.mixer},
        {"arp", m.arp},
    };
}

void from_json(const json& j, track_modulation_routing_snapshot_t& m) {
    j.at("modulations").get_to(m.effectMods);
    j.at("mixer").get_to(m.mixer);
    j.at("arp").get_to(m.arp);
}

namespace DAW::Shape {
void to_json(json& j, const shape_pt_t& m) {
    j = json{
        {"x", m.pos.x},
        {"y", m.pos.y},
        {"s", m.shape}
    };
}

void from_json(const json& j, shape_pt_t& m) {
    j.at("x").get_to(m.pos.x);
    j.at("y").get_to(m.pos.y);
    j.at("s").get_to(m.shape);
}

void to_json(json& j, const shape_t& m) {
    j = json{
        {"name", m.name},
        {"flags", m.flags},
        {"pts", m.pts}
    };
}

void from_json(const json& j, shape_t& m) {
    j.at("name").get_to(m.name);
    j.at("flags").get_to(m.flags);
    j.at("pts").get_to(m.pts);
}
} // namespace DAW::Shape
JSON_FROM_TO(layout_grid_t, offset, zoom)
void to_json(json& j, const layout_pianoroll_t& m) {
    j = json{
        {"offset", m.yoffset},
        {"scale", m.yscale},
        {"fold", m.bFoldNotes}
    };
}

void from_json(const json& j, layout_pianoroll_t& m) {
    j.at("offset").get_to(m.yoffset);
    j.at("scale").get_to(m.yscale);
    j.at("fold").get_to(m.bFoldNotes);
}
JSON_FROM_TO(clip_editor_layout_t, layoutGrid, layoutPianoRoll, noLayout)

void to_json(json& j, const clip_control_data_channel_t& m) {
    j = json{
        {"shape", m.shape}
    };
}

void from_json(const json& j, clip_control_data_channel_t& m) {
    j.at("shape").get_to(m.shape);
}

void to_json(json& j, const clip_control_data_t& m) {
    j = json{
        {"pitchBend", m.pitchBend},
        {"cc", m.ccChannels}
    };
}

void from_json(const json& j, clip_control_data_t& m) {
    j.at("pitchBend").get_to(m.pitchBend);
    j.at("cc").get_to(m.ccChannels);
}

void to_json(json& j, const clip_fade_t& m) {
    j = json{
        {"duration", m.durationMs},
        {"type", m.shape}
    };
}

void from_json(const json& j, clip_fade_t& m) {
    j.at("duration").get_to(m.durationMs);
    j.at("type").get_to(m.shape);
}

void to_json(json& j, const clip_audio_t& m) {
    j = json{
        {"id", m.id},
        {"fadeIn", m.fadeIn},
        {"fadeOut", m.fadeOut},
        {"pitch", m.settings.pitch},
        {"stretch", m.settings.stretch},
        {"flags", m.settings.flags}
    };
}

void from_json(const json& j, clip_audio_t& m) {
    j.at("id").get_to(m.id);
    if (j.find("fadeIn") != j.end()) {
        j.at("fadeIn").get_to(m.fadeIn);
    }
    if (j.find("fadeOut") != j.end()) {
        j.at("fadeOut").get_to(m.fadeOut);
    }
    if (m.fadeIn.shape.pts.empty()) 
        m.setDefaultFade(true);
    if (m.fadeOut.shape.pts.empty())
        m.setDefaultFade(false);
    if (j.find("pitch") != j.end()) {
        j.at("pitch").get_to(m.settings.pitch);
    }
    if (j.find("stretch") != j.end()) {
        j.at("stretch").get_to(m.settings.stretch);
    }
    if (j.find("flags") != j.end()) {
        j.at("flags").get_to(m.settings.flags);
    }
}

void to_json(json& j, const clip_notes_t& m) {
    auto shrdHeapVec = std::make_shared<std::vector<std::byte>>();
    shrdHeapVec->resize(64);
    DAW::ByteBuffer::stream_write<std::vector<std::byte>> out{*shrdHeapVec, 0};
    out.write(size_t(0));
    out.write(int32_t(1)); // version
    out.write(int32_t(0)); // reserved
    out.write(size_t(m.m_list.size()));
    for (const auto& note : m.m_list) {
        out.write<int64_t>(note.id);
        out.write<int32_t>(note.pitch);
        out.write<int32_t>(note.velocity);
        out.write<tick_t>(note.time);
        out.write<tick_t>(note.len);
        out.write<int32_t>(note.flags);
        out.write<int8_t>(note.channel);
        // write reserved bytes
        out.write<int8_t>(0);
        out.write<int8_t>(0);
        out.write<int8_t>(0);
    }
    out.setPos(0);
    out.write(size_t(shrdHeapVec->size()));
    // convert to base64:
    auto base64string = cereal::base64::encode(reinterpret_cast<const unsigned char *>( shrdHeapVec->data() ), shrdHeapVec->size());
    j["size"] = shrdHeapVec->size();
    j["data"] = base64string;
}

void from_json(const json& j, clip_notes_t& m) {
    size_t size = 0;
    j.at("size").get_to(size);
    std::string base64string;
    j.at("data").get_to(base64string);
    std::string strDec = cereal::base64::decode(base64string);
    // turn into byte vec
    std::vector<std::byte> vec;
    vec.resize(size);
    std::memcpy(vec.data(), strDec.data(), math::min(strDec.size(), size));
    DAW::ByteBuffer::stream_read in{vec};
    in.read(size);
    int32_t version = 0;
    int32_t reserved = 0;
    in.read(version);
    if (version != 1) {
        throw std::runtime_error("Invalid version");
    }
    in.read(reserved);
    if (version != 1) {
        throw std::runtime_error("Invalid version");
    }
    size_t numNotes = 0;
    in.read(numNotes);
    for (size_t i = 0; i < numNotes; ++i) {
        note_t note;
        in.read<int64_t>(note.id);
        in.read<int32_t>(note.pitch);
        in.read<int32_t>(note.velocity);
        in.read<tick_t>(note.time);
        in.read<tick_t>(note.len);
        in.read<int32_t>(note.flags);
        in.read<int8_t>(note.channel);
        m.m_list.push_back(note);
        in.skip(3);
    }

    m.updateBounds();
    m.removeDuplicates();
}

void to_json(json& j, const clip_t& m) {
    j = json{
        {"name", m.name},
        {"time", m.time},
        {"len", m.len},
        {"offset_start", m.offsetStart},
        {"loop_length", m.loopLen},
        {"enabled", m.enabled},
        {"rgb", m.rgb},
        {"loop_start", m.loopStart},
        {"loop_enabled", m.loopEnabled},
        {"editorLayout", m.editorLayout},
        {"clip_notes", m.notes},
        {"clip_audio", m.audio},
        {"type", m.clipType},
        {"len_samples", m.lenSamples},
        {"clip_data", m.controlData},
        {"groove", m.selectedGroove}
    };
}

void from_json(const json& j, clip_t& m) {
    j.at("name").get_to(m.name);
    j.at("time").get_to(m.time);
    j.at("len").get_to(m.len);
    j.at("offset_start").get_to(m.offsetStart);
    j.at("loop_length").get_to(m.loopLen);
    j.at("enabled").get_to(m.enabled);
    j.at("rgb").get_to(m.rgb);
    j.at("loop_start").get_to(m.loopStart);
    j.at("loop_enabled").get_to(m.loopEnabled);
    j.at("editorLayout").get_to(m.editorLayout);
    j.at("clip_notes").get_to(m.notes);
    j.at("clip_audio").get_to(m.audio);
    j.at("type").get_to(m.clipType);
    j.at("len_samples").get_to(m.lenSamples);
    if (j.find("clip_data") != j.end()) {
        j.at("clip_data").get_to(m.controlData);
    }
    if (j.find("groove") != j.end()) {
        j.at("groove").get_to(m.selectedGroove);
    }
}




void to_json(json& j, const plugin_snapshot_t& m) {
    j = json{
        {"version", 18},
        {"projectGlobalId", m.projectGlobalId},
        {"enabled", m.enabled},
        {"slot", m.slot},
        {"moduleType", m.moduleType},
        {"localDbId", m.localDbId},
        {"vendorVersion", m.vendorVersion},
        {"uId", m.uId},
        {"clapId", m.clapId},
        {"name", m.name},
        {"currentProgram", m.currentProgram},
        {"currentProgramName", m.currentProgramName},
        {"ioChannels", m.ioChannels},
        {"effectRouting", m.effectRouting},
        {"modulationRouting", m.modulationRouting},
        {"stageIds", m.stageIds},
        {"uiSnapshots", m.uiSnapshots},
        {"windowLayout", m.windowLayout},
        {"params", m.params},
        {"automatedParams", m.automatedParams},
        {"pluginSnapshots", m.pluginSnapshots}
    };
    auto jsonProgramData = json{
        {"data", cereal::base64::encode(reinterpret_cast<const unsigned char *>( m.dataChunk.data() ), m.dataChunk.size())},
        {"size", m.dataChunk.size()}
    };
    j["programData"] = jsonProgramData;
    auto jsonPluginData = json{
        {"data", cereal::base64::encode(reinterpret_cast<const unsigned char *>( m.dataChunk2.data() ), m.dataChunk2.size())},
        {"size", m.dataChunk2.size()}
    };
    j["pluginData"] = jsonPluginData;
}

void from_json(const json& j, plugin_snapshot_t& m) {
    if (j.find("version") == j.end()) {
        m.version = 0;
        return;
    }
    j.at("version").get_to(m.version);
    j.at("projectGlobalId").get_to(m.projectGlobalId);
    j.at("enabled").get_to(m.enabled);
    j.at("slot").get_to(m.slot);
    j.at("moduleType").get_to(m.moduleType);
    j.at("localDbId").get_to(m.localDbId);
    j.at("vendorVersion").get_to(m.vendorVersion);
    j.at("uId").get_to(m.uId);
    j.at("clapId").get_to(m.clapId);
    j.at("name").get_to(m.name);
    j.at("currentProgram").get_to(m.currentProgram);
    j.at("currentProgramName").get_to(m.currentProgramName);
    j.at("ioChannels").get_to(m.ioChannels);
    j.at("effectRouting").get_to(m.effectRouting);
    j.at("modulationRouting").get_to(m.modulationRouting);
    j.at("stageIds").get_to(m.stageIds);
    j.at("uiSnapshots").get_to(m.uiSnapshots);
    j.at("windowLayout").get_to(m.windowLayout);
    j.at("params").get_to(m.params);
    j.at("automatedParams").get_to(m.automatedParams);
    j.at("pluginSnapshots").get_to(m.pluginSnapshots);
    auto jsonProgramData = j.at("programData");
    auto base64string = jsonProgramData.at("data").get<std::string>();
    auto decoded = cereal::base64::decode(base64string);
    if (decoded.size() != jsonProgramData.at("size").get<size_t>()) {
        throw std::runtime_error("Invalid data size");
    }
    m.dataChunk.resize(decoded.size());
    std::memcpy(m.dataChunk.data(), decoded.data(), decoded.size());
    auto jsonPluginData = j.at("pluginData");
    base64string = jsonPluginData.at("data").get<std::string>();
    decoded = cereal::base64::decode(base64string);
    if (decoded.size() != jsonPluginData.at("size").get<size_t>()) {
        throw std::runtime_error("Invalid data size");
    }
    m.dataChunk2.resize(decoded.size());
    std::memcpy(m.dataChunk2.data(), decoded.data(), decoded.size());
}

void to_json(json& j, const track_impl_snapshot_t& m) {
    j = json{
        {"plugins", m.pluginSnapshots},
        {"track", m.trackParams},
        {"arp", m.trackArp},
        {"io", m.trackIO},
        {"routing", m.effectRouting},
        {"modulation", m.modulationRouting}
    };
}

void from_json(const json& j, track_impl_snapshot_t& m) {
    j.at("plugins").get_to(m.pluginSnapshots);
    j.at("track").get_to(m.trackParams);
    j.at("arp").get_to(m.trackArp);
    j.at("io").get_to(m.trackIO);
    j.at("routing").get_to(m.effectRouting);
    j.at("modulation").get_to(m.modulationRouting);
}

void to_json(json& j, const tracksettings_t& m) {
    j = json{
        {"name", m.name},
        {"rgb", m.rgb},
        {"type", m.type}
    };
}

void from_json(const json& j, tracksettings_t& m) {
    j.at("name").get_to(m.name);
    j.at("rgb").get_to(m.rgb);
    j.at("type").get_to(m.type);
}

void to_json(json& j, const tracksnapshot_store_opts_t& m) {
    j = json{
        {"store_plugin_preset", m.storePluginPreset},
        {"store_automation", m.storeAutomation},
        {"store_clips", m.storeClips},
        {"store_layouts", m.storeLayouts}
    };
}

void from_json(const json& j,  tracksnapshot_store_opts_t& m) {
    j.at("store_plugin_preset").get_to(m.storePluginPreset);
    j.at("store_automation").get_to(m.storeAutomation);
    j.at("store_clips").get_to(m.storeClips);
    j.at("store_layouts").get_to(m.storeLayouts);
}

void to_json(json& j, const project_globals_t& m) {
    j = json{
        {"loopEnabled", m.loopEnabled},
        {"loopStart", m.loopStart},
        {"loopLen", m.loopLen},
        {"tempo100", m.tempo100},
        {"signatureNum", m.signatureNum},
        {"signatureDenom", m.signatureDenom},
        {"playbackPos", m.playbackPos},
        {"cursor", m.cursor},
        {"grooveData", m.grooveData}
    };
}

void from_json(const json& j, project_globals_t& m) {
    j.at("loopEnabled").get_to(m.loopEnabled);
    j.at("loopStart").get_to(m.loopStart);
    j.at("loopLen").get_to(m.loopLen);
    j.at("tempo100").get_to(m.tempo100);
    j.at("signatureNum").get_to(m.signatureNum);
    j.at("signatureDenom").get_to(m.signatureDenom);
    j.at("playbackPos").get_to(m.playbackPos);
    j.at("cursor").get_to(m.cursor);
    j.at("grooveData").get_to(m.grooveData);
}
void to_json(json& j, const track_snapshot_t& m) {
    j = json{
        {"idx", m.localIdx},
        {"settings", m.trackSettings},
        {"clips", m.clips},
        {"data", m.data},
        {"stage_ids", m.stageIds},
        {"store_opts", m.storeOpts},
        {"layouts", m.layouts},
        {"layouts_mixer", m.layoutsMixer}
    };
}

void from_json(const json& j, track_snapshot_t& m) {
    j.at("idx").get_to(m.localIdx);
    j.at("settings").get_to(m.trackSettings);
    j.at("clips").get_to(m.clips);
    j.at("data").get_to(m.data);
    j.at("stage_ids").get_to(m.stageIds);
    j.at("store_opts").get_to(m.storeOpts);
    j.at("layouts").get_to(m.layouts);
    j.at("layouts_mixer").get_to(m.layoutsMixer);
}

void to_json(json& j, const trackcontainer_snapshot_t& m) {
    j = json{
        {"version", 2},
        {"tracklist", m.tracks},
        {"hierachy", m.hierachy}
    };
}

void from_json(const json& j, trackcontainer_snapshot_t& m) {
    if (j.find("version") == j.end()) {
        m.version = 0;
        return;
    }
    j.at("version").get_to(m.version);
    j.at("tracklist").get_to(m.tracks);
    j.at("hierachy").get_to(m.hierachy);
}

void to_json(json& j, const groove_file_t& m) {
    j = json{
        {"version", 2},
        {"grooves", m.grooves}
    };
}

void from_json(const json& j, groove_file_t& m) {
    if (j.find("version") == j.end()) {
        m.version = 0;
        return;
    }
    j.at("version").get_to(m.version);
    j.at("grooves").get_to(m.grooves);
}

void to_json(json& j, const project_snapshot_t& m) {
    j = json{
        {"masterTracks", m.trackMasterCtr},
        {"returnTracks", m.trackReturnCtr},
        {"tracks", m.trackCtr},
        {"globals", m.globals},
        {"exportSettings", m.exportSettings},
        {"quantizeSettings", m.quantizeSettings},
        {"samplerate", m.samplerate},
        {"solo", m.solodTracks},
        {"record", m.recordArmedTracks}
    };
}

void from_json(const json& j, project_snapshot_t& m) {
    j.at("masterTracks").get_to(m.trackMasterCtr);
    j.at("returnTracks").get_to(m.trackReturnCtr);
    j.at("tracks").get_to(m.trackCtr);
    j.at("globals").get_to(m.globals);
    j.at("exportSettings").get_to(m.exportSettings);
    j.at("quantizeSettings").get_to(m.quantizeSettings);
    j.at("samplerate").get_to(m.samplerate);
    j.at("solo").get_to(m.solodTracks);
    j.at("record").get_to(m.recordArmedTracks);
}

void to_json(json& j, const project_layout_t& m) {
    j = json{
        {"grid", m.layoutGrid},
        {"scrollOffsetX", m.scrollOffsetX}
    };
}

void from_json(const json& j, project_layout_t& m) {
    j.at("grid").get_to(m.layoutGrid);
    j.at("scrollOffsetX").get_to(m.scrollOffsetX);
}
namespace nlohmann
{
template <typename T>
struct adl_serializer<std::shared_ptr<T>>
{
    static void to_json(json& j, const std::shared_ptr<T>& opt)
    {
        if (opt)
        {
            j = *opt;
        }
        else
        {
            j = nullptr;
        }
    }

    static void from_json(const json& j, std::shared_ptr<T>& opt)
    {
        if (j.is_null())
        {
            opt = nullptr;
        }
        else
        {
            opt.reset(new T(j.get<T>()));
        }
    }
};
}
void to_json(json& j, const guictrlayout_entry_snapshot_t& m) {
    j = json{
        {"type", m.type},
        {"label", m.label},
        {"ctrLayout", m.ctrLayout},
        {"activePosition", m.activePosition},
        {"entries", m.entries},
        {"splitterPositions", m.splitterPositions},
        {"entryTag", m.entryTag},
        {"data", m.data}
    };
}

void from_json(const json& j, guictrlayout_entry_snapshot_t& m) {
    j.at("type").get_to(m.type);
    j.at("label").get_to(m.label);
    j.at("ctrLayout").get_to(m.ctrLayout);
    j.at("activePosition").get_to(m.activePosition);
    j.at("entries").get_to(m.entries);
    j.at("splitterPositions").get_to(m.splitterPositions);
    j.at("entryTag").get_to(m.entryTag);
    j.at("data").get_to(m.data);
}

void to_json(json& j, const dawview_layout_t& m) {
    j = json{
        {"version", 3},
        {"splitterPositions", m.splitterPositions},
        {"sidebarSnapshots", m.sidebarSnapshots},
        {"sidebarSelected", m.sidebarSelected},
        {"right", m.right},
        {"center", m.center}
    };
}

void from_json(const json& j, dawview_layout_t& m) {
    if (j.find("version") == j.end()) {
        m.version = 0;
        return;
    }
    j.at("version").get_to(m.version);
    j.at("splitterPositions").get_to(m.splitterPositions);
    j.at("right").get_to(m.right);
    j.at("center").get_to(m.center);
    if (m.version >= 3) {
        j.at("sidebarSnapshots").get_to(m.sidebarSnapshots);
        j.at("sidebarSelected").get_to(m.sidebarSelected);
    }
}
JSON_FROM_TO(samplefile_entry_t, id, name)
JSON_FROM_TO(samplefile_index_t, list)

void to_json(json& j, const project_file& m) {
    j = json{
        {"version", 3},
        {"projectdata", m.project},
        {"layout", m.layout},
        {"samples", m.sampleFileIndex},
        {"layouts", m.layouts}
    };
}

void from_json(const json& j, project_file& m) {
    if (j.find("version") != j.end()) {
        j.at("version").get_to(m.fileFmtVersion);
        j.at("projectdata").get_to(m.project);
        j.at("layout").get_to(m.layout);
        j.at("samples").get_to(m.sampleFileIndex);
        j.at("layouts").get_to(m.layouts);
    } else {
        m.fileFmtVersion = 0;
    }
}

namespace DAW::ProjectFileV2 {
    std::optional<String> saveProject(const std::shared_ptr<project_file>& f, std::vector<uint8_t>& bufferOut) {
        try {
            Stringstream sstream;
            {
                json j;
                to_json(j, *f.get());
                sstream << j.dump(4);
            }
            sstream.flush();
            bufferOut.assign(std::istreambuf_iterator<char>(sstream), std::istreambuf_iterator<char>());
            return std::nullopt;
        } catch (const std::exception& e) {
            return e.what();
        }
    }
    
    std::variant<std::shared_ptr<project_file>, String> loadProject(const std::vector<uint8_t>& vec) {
        try {
            auto str = std::string(vec.begin(), vec.end());
            auto f = std::make_shared<project_file>();
            {
                json j = json::parse(str);
                from_json(j, *f.get());
            }
            if (f->fileFmtVersion < 3) {
                return DAW::ProjectFileV1::loadProject(vec);
            }
            if (f->project.samplerate == 0) {
                f->project.samplerate = 44100;
                log_lf(Log::L_WARN, "legacy project file without samplerate, setting samplerate to %u\n", 44100);
            }
            return f;
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    std::optional<String> saveProjectToJsonFile(const std::shared_ptr<project_file>& f, const String& path) {
        try {
            std::vector<uint8_t> buf;
            auto ret = saveProject(f, buf);
            if (ret) {
                return ret;
            }
            WriteFileVector(path, buf);
            return std::nullopt;
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    std::variant<std::shared_ptr<project_file>, String> loadProjectFromJsonFile(const String& path) {
        try {
            std::vector<uint8_t> vec;
            ReadFileVector(path, vec);
            auto f = loadProject(vec);
            if (std::holds_alternative<std::shared_ptr<project_file>>(f)) {
                return std::get<std::shared_ptr<project_file>>(f);
            } else {
                return std::get<String>(f);
            }
        } catch (const std::exception& e) {
            return e.what();
        }
    }
    
    std::optional<String> saveTrackContainer(const trackcontainer_snapshot_t& container, const String& path) {
        try {
            Stringstream sstream;
            {
                json j;
                to_json(j, container);
                sstream << j.dump(4);
            }
            sstream.flush();
            writeStringStream(path, sstream);
            return std::nullopt;
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    std::variant<std::shared_ptr<trackcontainer_snapshot_t>, String> loadTrackContainer(const String& path) {
        try {
            std::vector<uint8_t> vec;
            ReadFileVector(path, vec);
            auto str = std::string(vec.begin(), vec.end());
            std::shared_ptr<trackcontainer_snapshot_t> snapshot = std::make_shared<trackcontainer_snapshot_t>();
            {
                json j = json::parse(str);
                from_json(j, *snapshot.get());
            }
            if (snapshot->version < 2) {
                return DAW::ProjectFileV1::loadTrackContainer(path);
            }
            return snapshot;
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    std::optional<String> serializePluginSnapshot(const plugin_snapshot_t& snapshot, std::vector<uint8_t>& buf) {
        try {
            Stringstream sstream;
            {
                json j;
                to_json(j, snapshot);
                sstream << j.dump(4);
            }
            sstream.flush();
            buf.assign(std::istreambuf_iterator<char>(sstream), std::istreambuf_iterator<char>());
            return std::nullopt;
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    std::variant<std::shared_ptr<plugin_snapshot_t>, String> deserializePluginSnapshot(std::vector<uint8_t>& vec) {
        try {
            std::shared_ptr<plugin_snapshot_t> snapshot = std::make_shared<plugin_snapshot_t>();
            {
                json j = json::parse(std::string(vec.begin(), vec.end()));
                from_json(j, *snapshot.get());
                if (snapshot->version < 18) {
                    return DAW::ProjectFileV1::deserializePluginSnapshot(vec);
                }
            }
            return snapshot;
        } catch (const std::exception& e) {
            return e.what();
        }
    }
    
    std::optional<String> savePluginSnapshot(const plugin_snapshot_t& snapshot, const String& path) {
        try {
            Stringstream sstream;
            {
                json j;
                to_json(j, snapshot);
                sstream << j.dump(4);
            }
            sstream.flush();
            writeStringStream(path, sstream);
            return std::nullopt;
        } catch (const std::exception& e) {
            return e.what();
        }
    }
    
    std::variant<std::shared_ptr<plugin_snapshot_t>, String> loadPluginSnapshot(const String& path) {
        try {
            std::vector<uint8_t> vec;
            ReadFileVector(path, vec);
            return deserializePluginSnapshot(vec);
        } catch (const std::exception& e) {
            return e.what();
        }
    }
    
    std::optional<String> saveGrooveFile(const groove_file_t& grooveFile, const String& path) {
        try {
            Stringstream sstream;
            {
                json j;
                to_json(j, grooveFile);
                sstream << j.dump(4);
            }
            sstream.flush();
            writeStringStream(path, sstream);
            return std::nullopt;
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    std::variant<groove_file_t, String> loadGrooveFile(const String& path) {
        try {
            groove_file_t grooveFile;
            std::vector<uint8_t> vec;
            ReadFileVector(path, vec);
            {
                json j = json::parse(std::string(vec.begin(), vec.end()));
                from_json(j, grooveFile);
            }
            if (grooveFile.version < 1) {
                return DAW::ProjectFileV1::loadGrooveFile(path);
            }
            return grooveFile;
        } catch (const std::exception& e) {
            return e.what();
        }
    }

    std::optional<String> saveDawViewLayoutSnapshot(dawview_layout_t& snapshot, const String& path) {
        using namespace cereal;
        try {
            Stringstream sstream;
            {
                json j;
                to_json(j, snapshot);
                sstream << j.dump(4);
            }
            sstream.flush();
            writeStringStream(App::Platform::toUserdataPath("data/" + path), sstream);
            return std::nullopt;
        } catch (const std::exception& e) {
            return e.what();
        }
    }
    
    std::variant<std::shared_ptr<dawview_layout_t>, String> loadDawViewLayoutSnapshot(const String& path) {
        using namespace cereal;
        try {
            std::vector<uint8_t> vec;
            String fileUserDataPath = App::Platform::toUserdataPath("data/" + path);
            if (FileExists(fileUserDataPath)) {
                ReadFileVector(fileUserDataPath, vec);
            } else {
                String fileTemplateProgramPath = App::Platform::toDefaultSettingFilesPath(path);
                if (FileExists(fileTemplateProgramPath)) {
                    ReadFileVector(fileTemplateProgramPath, vec);
                } else {
                    return "File not found: " + path;
                }
            }
            Stringstream sstream(std::string(vec.cbegin(), vec.cend()));
            std::shared_ptr<dawview_layout_t> snapshot = std::make_shared<dawview_layout_t>();
            dawview_layout_t& ref = *snapshot.get();
            {
                json j = json::parse(sstream);
                from_json(j, ref);
            }
            if (ref.version < 2) {
                return DAW::ProjectFileV1::loadDawViewLayoutSnapshot(path);
            }
            return snapshot;
        } catch (const std::exception& e) {
            return e.what();
        }
    }
} // namespace DAW::ProjectFileV2
