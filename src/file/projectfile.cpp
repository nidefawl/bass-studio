#include "projectfile.h"
#include "shapefile.h"

#include "snapshot/snapshot.h"
#include "snapshot/trackrouting-snapshot.h"
#include "snapshot/track-snapshot.h"
#include "snapshot/plugin-snapshot.h"
#include "snapshot/project-snapshot.h"
#include "gui/container/container_layout_types.h"

#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <functional>

#include "config.h"
#include "exceptions.h"
#include "seq_time.h"
#include "seq_util.h"
#include "math/vec.h"
#include "str_util.h"
#include "fileio.h"
#include "logging.h"

#include "clip.h"
#include "host/daw_channel.h"
#include "automation.h"
#include "track.h"
#include "layout.h"
#include "project.h"

#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal_optional_nvp/cereal_optional_nvp.hpp>


using namespace cereal;

namespace DAW {
template<class Archive>
void serialize(Archive& archive, channel_desc& m) {
    archive(make_nvp("input", m.name), make_nvp("count", m.count), make_nvp("offset", m.offset));
}
template<class Archive>
void serialize(Archive& archive, modulation_scaling_t& m) {
    archive(make_nvp("min", m.min),
        make_nvp("max", m.max));
    make_optional_nvp(archive, "mode", m.mode);
    make_optional_nvp(archive, "clamp", m.bClamp);
}
template<class Archive>
void serialize(Archive& archive, modulation_channel_ref& m) {
    archive(make_nvp("idx", m.paramIdxDst),
        make_nvp("ref", m.refSrc),
        make_nvp("scale", m.scale));
}
}

template<class Archive>
void serialize(Archive& archive, plugin_iodesc_snapshot_t& m) {
    archive(make_nvp("input", m.input), make_nvp("output", m.output));
}

template<class Archive>
void serialize(Archive& archive, param_snapshot_t& m) {
    archive(make_nvp("idx", m.idx), make_nvp("val", m.val), make_nvp("flags", m.flags));
}

template<class Archive>
void load(Archive& archive, automatable_param_ref_t& m, const std::uint32_t version) {
    if (version == 0) {
        int dummy = 0;
        archive(make_nvp("type", m.type),
            make_nvp("paramIdx", m.paramIdx),
            make_nvp("height", dummy),
            make_nvp("refId", m.refId));
    } else {
        archive(make_nvp("type", m.type),
            make_nvp("paramIdx", m.paramIdx),
            make_nvp("refId", m.refId));
    }
}
template<class Archive>
void save(Archive& archive, const automatable_param_ref_t& m, const std::uint32_t version) {
    archive(make_nvp("type", m.type),
        make_nvp("paramIdx", m.paramIdx),
        make_nvp("refId", m.refId));
}
template<class Archive>
void serialize(Archive& archive, automation_view_t& m) {
    archive(make_nvp("param", m.targetParam),
        make_nvp("data", m.points),
        make_nvp("active", m.active));
}

namespace glm
{
template<class Archive>
void serialize(Archive& archive, glm::ivec4& m) {
    archive(make_nvp("x", m.x), make_nvp("y", m.y), make_nvp("w", m.z), make_nvp("h", m.w));
}
} // namespace glm

template<class Archive>
void serialize(Archive& archive, plugin_ui_snapshot_t& m) {
    try {
        archive(
            make_nvp("windowPosSize", m.windowPosSize),
            make_nvp("windowPosValid", m.windowPosSizeValid),
            make_nvp("windowOpen", m.isWindowOpen),
            make_nvp("layoutMode", m.layoutMode),
            make_nvp("parameterListVisible", m.parameterListVisible));
    } catch (const std::exception& e) {
        m = {};
    }
}

template<class Archive>
void load(Archive& archive, plugin_snapshot_t& m, const std::uint32_t version) {
    m.version = version;
    archive(
        make_nvp("pluginType", m.pluginType),
        make_nvp("name", m.name),
        make_nvp("uId", m.uId)
    );
    if (version >= 12) {
        archive(make_nvp("clapId", m.clapId));
    }
    archive(
        make_nvp("slot", m.slot),
        make_nvp("parameters", m.params),
        make_nvp("automatedParams", m.automatedParams),
        make_nvp("globalId", m.projectGlobalId),
        make_nvp("enabled", m.enabled));
    {
        size_type size;
        archive(make_nvp("sizeprogramdata", size));
        m.dataChunk2.resize(size);
        ((JSONInputArchive*) &archive)->loadBinaryValue((void*) m.dataChunk2.data(), size, "programdata");
    }
    {
        size_type size;
        archive(make_nvp("sizeplugindata", size));
        m.dataChunk.resize(size);
        ((JSONInputArchive*) &archive)->loadBinaryValue((void*) m.dataChunk.data(), size, "plugindata");
    }
    archive(make_nvp("vendorVersion", m.vendorVersion),
        make_nvp("localDbId", m.localDbId),
        make_nvp("programIdx", m.currentProgram),
        make_nvp("programName", m.currentProgramName));
    if (version >= 9) {
        archive(make_nvp("ioChannels", m.ioChannels), make_nvp("version", m.version));
    }
    if (version >= 10) {
        archive(make_nvp("stageIds", m.stageIds), make_nvp("routing", m.effectRouting));
    }
    if (version >= 11) {
        archive(make_nvp("ui", m.uiSnapshot));
    }
    archive(make_nvp("plugins", m.pluginSnapshots));
}

template<class Archive>
void save(Archive& archive, plugin_snapshot_t const& m, const std::uint32_t version) {
    archive(make_nvp("pluginType", m.pluginType),
        make_nvp("name", m.name), 
        make_nvp("uId", m.uId), 
        make_nvp("clapId", m.clapId), 
        make_nvp("slot", m.slot),
        make_nvp("parameters", m.params),
        make_nvp("automatedParams", m.automatedParams),
        make_nvp("globalId", m.projectGlobalId),
        make_nvp("enabled", m.enabled));
    {
        size_type size = m.dataChunk2.size();
        archive(make_nvp("sizeprogramdata", size));
        ((JSONOutputArchive*) &archive)->saveBinaryValue(m.dataChunk2.data(), size, "programdata");
    }
    {
        size_type size = m.dataChunk.size();
        archive(make_nvp("sizeplugindata", size));
        ((JSONOutputArchive*) &archive)->saveBinaryValue(m.dataChunk.data(), size, "plugindata");
    }
    archive(
        make_nvp("vendorVersion", m.vendorVersion),
        make_nvp("localDbId", m.localDbId),
        make_nvp("programIdx", m.currentProgram),
        make_nvp("programName", m.currentProgramName),
        make_nvp("ioChannels", m.ioChannels),
        make_nvp("version", m.version),
        make_nvp("stageIds", m.stageIds),
        make_nvp("routing", m.effectRouting),
        make_nvp("ui", m.uiSnapshot),
        make_nvp("plugins", m.pluginSnapshots)
    );
}

template<class Archive>
void serialize(Archive& archive, track_params_snapshot_t& m) {
    archive(make_nvp("params", m.params), make_nvp("automation", m.automatedParams));
}

template<class Archive>
void serialize(Archive& archive, arp_snapshot& m) {
    archive(make_nvp("params", m.params), make_nvp("automation", m.automatedParams));
}

template<class Archive>
void serialize(Archive& archive, io_configuration_snapshot_t& m) {
    try {
        archive(
            make_nvp("type", m.type),
            make_nvp("stageId", m.stageId),
            make_nvp("stageEndPointType", m.stageEndPointType),
            make_nvp("externalInputType", m.externalInputType),
            make_nvp("projectGlobalId", m.projectGlobalId),
            make_nvp("externalInputIdx", m.externalInputIdx),
            make_nvp("srcChannelOffset", m.srcChannelOffset),
            make_nvp("dstChannelOffset", m.dstChannelOffset)
        );
    } catch (const std::exception& e) {
        m = {};
        log_lf(Log::L_WARN, "Failed loading io_configuration_snapshot_t: %s\n", e.what());
    }
}
template<class Archive>
void serialize(Archive& archive, io_midi_snapshot_t& m) {
    archive(
        make_nvp("stageId", m.stageId),
        make_nvp("stageEndPointType", m.stageEndPointType)
    );
    make_optional_nvp(archive, "type", m.type);
    make_optional_nvp(archive, "externalInputIdx", m.externalInputIdx);
}

template<class Archive>
void serialize(Archive& archive, track_io_configuration_snapshot_t& m) {
    archive(make_nvp("input", m.input), make_nvp("output", m.output));
    make_optional_nvp(archive, "midiInput", m.midiInput);
}

template<class Archive>
void serialize(Archive& archive, track_effect_routing_snapshot_t& m) {
    archive(
        make_nvp("inputRoutingOutputStage", m.inputRoutingOutputStage),
        make_nvp("inputRoutingEffects", m.inputRoutingEffects),
        make_nvp("routingState", m.routingState)
    );
}

template<class Archive>
void serialize(Archive& archive, track_modulation_routing_snapshot_t& m) {
    archive(make_nvp("modulations", m.effectMods));
    make_optional_nvp(archive, "mixer", m.mixer);
    make_optional_nvp(archive, "arp", m.arp);
}

template<class Archive>
void serialize(Archive& archive, track_impl_snapshot_t& m) {
    archive(make_nvp("plugins", m.pluginSnapshots),
        make_nvp("track", m.trackParams),
        make_nvp("arp", m.trackArp),
        make_nvp("io", m.trackIO),
        make_nvp("routing", m.effectRouting));
    make_optional_nvp(archive, "modulation", m.modulationRouting);
}

template<class Archive>
void save(Archive& archive, tracksettings_t const& m, const std::uint32_t version) {
    archive(make_nvp("name", m.name),
            make_nvp("rgb", m.rgb),
            make_nvp("type", m.type));
}

template<class Archive>
void load(Archive& archive, tracksettings_t& m, const std::uint32_t version) {
    archive(make_nvp("name", m.name),
            make_nvp("rgb", m.rgb),
            make_nvp("type", m.type));
}

template<class Archive>
void serialize(Archive& archive, tracklayout_settings_t& m) {
    archive(make_nvp("height", m.height),
            make_nvp("hideSubtracks", m.hideSubtracks),
            make_nvp("hideTrack", m.hideTrack));
}

template<class Archive>
void serialize(Archive& archive, subtracksettings_t& m) {
    archive(make_nvp("subtrackType", m.subtrackType));
}

template<class Archive>
void serialize(Archive& archive, subtracklayout_settings_t& m) {
    archive(make_nvp("subtrackType", m.height));
}

template<class Archive>
void serialize(Archive& archive, subtrack_snapshot_t& m) {
    archive(make_nvp("settings", m.settings),
        make_nvp("layoutSettings", m.layoutSettings),
        make_nvp("automationLane", m.atlRef));
}

template<class Archive>
void save(Archive& archive, track_layout_snapshot_t const& m, const std::uint32_t version) {
    archive(make_nvp("layout", m.layout),
            make_nvp("subtracks", m.subtracks));
}

template<class Archive>
void load(Archive& archive, track_layout_snapshot_t& m, const std::uint32_t version) {
    archive(make_nvp("layout", m.layout));
    if (version > 0) {
        archive(make_nvp("subtracks", m.subtracks));
    }
}

template<class Archive>
void serialize(Archive& archive, track_id_snapshot_t& m) {
    archive(make_nvp("stageId", m.stageId),
            make_nvp("inputStageId", m.inputStageId),
            make_nvp("outputStageId", m.outputStageId),
            make_nvp("outputPostStageId", m.outputPostStageId));
}

template<class Archive>
void serialize(Archive& archive, tracksnapshot_store_opts_t& m) {
    archive(
        make_nvp("storePluginPreset", m.storePluginPreset),
        make_nvp("storeAutomation", m.storeAutomation),
        make_nvp("storeClips", m.storeClips),
        make_nvp("storeLayouts", m.storeLayouts)
    );
}

template<class Archive>
void load(Archive& archive, track_snapshot_t& m, const std::uint32_t version) {
    if (version == 0) {
        make_optional_nvp(archive, "idx", m.localIdx);
        archive(
            make_nvp("settings", m.trackSettings),
            make_nvp("clips", m.clips),
            make_nvp("plugins", m.data)
        );
        int32_t stageId = m.stageIds.stageId;
        make_optional_nvp(archive, "stageId", stageId);
        if (stageId >= 0) {
            stageId *= 4;
            m.stageIds.stageId = stageId++;
            m.stageIds.inputStageId = stageId++;
            m.stageIds.outputStageId = stageId++;
            m.stageIds.outputPostStageId = stageId++;
        }
    } else {
        archive(
            make_nvp("idx", m.localIdx),
            make_nvp("settings", m.trackSettings),
            make_nvp("clips", m.clips)
        );
        if (version < 2) {
            archive(make_nvp("plugins", m.data));
        } else {
            archive(make_nvp("data", m.data));
        }
        archive(make_nvp("stageIds", m.stageIds));
        
        if (version >= 3) {
            archive(make_nvp("storeOpts", m.storeOpts));
        } else {
            m.storeOpts = tracksnapshot_store_opts_t::All();
        }
        if (version >= 4) {
            archive(make_nvp("layouts", m.layouts));
        }
    }
}

template<class Archive>
void save(Archive& archive, const track_snapshot_t& m, const std::uint32_t version) {
    archive(
        make_nvp("idx", m.localIdx),
        make_nvp("settings", m.trackSettings),
        make_nvp("clips", m.clips),
        make_nvp("data", m.data),
        make_nvp("stageIds", m.stageIds),
        make_nvp("storeOpts", m.storeOpts),
        make_nvp("layouts", m.layouts)
    );
}

template<class Archive>
void serialize(Archive& archive, trackcontainer_snapshot_t& m) {
    archive(make_nvp("tracklist", m.tracks), make_nvp("hierachy", m.hierachy));
}

template<class Archive>
void serialize(Archive& archive, layout_grid_t& m) {
    archive(make_nvp("offset", m.offset),
            make_nvp("zoom", m.zoom));
}

template<class Archive>
void serialize(Archive& archive, layout_pianoroll_t& m) {
    archive(make_nvp("offset", m.yoffset),
            make_nvp("scale", m.yscale));
    make_optional_nvp(archive, "fold", m.fold);
}

template<class Archive>
void serialize(Archive& archive, clip_editor_layout_t& m) {
    archive(make_nvp("layoutGrid", m.layoutGrid),
            make_nvp("layoutPianoRoll", m.layoutPianoRoll));
}

template<class Archive>
void serialize(Archive& archive, automation_point_t& m) {

    archive(make_nvp("time", m.time),
            make_nvp("val", m.val));
}

template<class Archive>
void serialize(Archive& archive, clip_control_data_channel_t& m) {
    archive(make_nvp("shape", m.shape));
}

template<class Archive>
void serialize(Archive& archive, clip_control_data_t& m) {
    archive(make_nvp("pitchBend", m.pitchBend), make_nvp("cc", m.ccChannels));
}

template<class Archive>
void serialize(Archive& archive, clip_t& m) {
    archive(make_nvp("name", m.name),
            make_nvp("time", m.time),
            make_nvp("len", m.len),
            make_nvp("offsetStart", m.offsetStart),
            make_nvp("loopLen", m.loopLen),
            make_nvp("enabled", m.enabled),
            make_nvp("rgb", m.rgb),
            make_nvp("loopStart", m.loopStart),
            make_nvp("loopEnabled", m.loopEnabled),
            make_nvp("noLayout", m.noLayout),
            make_nvp("editorLayout", m.editorLayout),
            make_nvp("clip_notes", m.notes),
            make_nvp("clip_audio", m.audio),
            make_nvp("type", m.clipType),
            make_nvp("lenSamples", m.lenSamples));
    make_optional_nvp(archive, "clip_data", m.controlData);
    if (m.loopLen == 0) {
        m.loopStart = m.offsetStart;
        m.loopLen = m.len;
    }
}

template<class Archive>
void save(Archive& archive, clip_notes_t const& m) {
    archive(make_nvp("notes", m.m_list));
}

template<class Archive>
void load(Archive& archive, clip_notes_t& m) {
    archive(make_nvp("notes", m.m_list));
    m.updateBounds();
    m.removeDuplicates();
}

template<class Archive>
void serialize(Archive& archive, clip_fade_t& m) {
    archive(make_nvp("duration", m.durationMs), make_nvp("type", m.shape));
}

template<class Archive>
void save(Archive& archive, clip_audio_t const& m) {
    archive(make_nvp("id", m.id), make_nvp("fadeIn", m.fadeIn), make_nvp("fadeOut", m.fadeOut));
}

template<class Archive>
void load(Archive& archive, clip_audio_t& m) {
    archive(make_nvp("id", m.id));
    make_optional_nvp(archive, "fadeIn", m.fadeIn);
    make_optional_nvp(archive, "fadeOut", m.fadeOut);
}

//TODO: don't archive each note seperately
template<class Archive>
void save(Archive& archive, note_t const& m) {
    archive(make_nvp("time", m.time),
            make_nvp("len", m.len),
            make_nvp("pitch", m.pitch),
            make_nvp("flags", m.flags));
    float fVel = m.velocity;
    archive(make_nvp("velocity", fVel));
}

template<class Archive>
void load(Archive& archive, note_t& m) {
    archive(make_nvp("time", m.time),
            make_nvp("len", m.len),
            make_nvp("pitch", m.pitch),
            make_nvp("flags", m.flags));
    float fVel = 0;
    if (make_optional_nvp(archive, "velocity", fVel)) {
        m.velocity = CLAMP_I(static_cast<int32_t>(fVel), 0, 127);
    }
}

template<class Archive>
void serialize(Archive& archive, project_globals_t& m) {
    archive(make_nvp("loopEnabled", m.loopEnabled),
            make_nvp("loopStart", m.loopStart),
            make_nvp("loopLen", m.loopLen),
            make_nvp("tempo100", m.tempo100),
            make_nvp("signatureNum", m.signatureNum),
            make_nvp("signatureDenom", m.signatureDenom),
            make_nvp("playbackPos", m.playbackPos),
            make_nvp("cursor", m.cursor));
}

template<class Archive>
void serialize(Archive& archive, export_settings_t& m) {
    archive(make_nvp("pos", m.exportPos),
            make_nvp("len", m.exportLen),
            make_nvp("path", m.exportPath),
            make_nvp("locked", m.isLocked));
}

template<class Archive>
void serialize(Archive& archive, quantize_settings& m) {
    archive(make_nvp("start", m.quantizeStart),
            make_nvp("end", m.quantizeEnd));
}

template<class Archive>
void serialize(Archive& archive, project_snapshot_t& m) {
    archive(make_nvp("masterTracks", m.trackMasterCtr),
            make_nvp("returnTracks", m.trackReturnCtr),
            make_nvp("tracks", m.trackCtr),
            make_nvp("globals", m.globals),
            make_nvp("exportSettings", m.exportSettings));
    make_optional_nvp(archive, "quantizeSettings", m.quantizeSettings);
    make_optional_nvp(archive, "samplerate", m.samplerate);
};

template<class Archive>
void serialize(Archive& archive, project_layout_t& m) {
    archive(make_nvp("grid", m.layoutGrid),
            make_nvp("scrollOffsetX", m.scrollOffsetX));
};

namespace DAW {
    template<class Archive>
    void serialize(Archive& archive, Cursor& m) {
        archive(make_nvp("pos", m.cursorPos),
                make_nvp("track", m.cursorTrack),
                make_nvp("subtrack", m.cursorSubTrack),
                make_nvp("range", m.selRange),
                make_nvp("trackrange", m.selTrackRange),
                make_nvp("subtrackrange", m.selSubTrackRange));
    };
}// namespace DAW

template<class Archive>
void serialize(Archive& archive, samplefile_index_t& m) {
    archive(make_nvp("list", m.list));
};

template<class Archive>
void serialize(Archive& archive, samplefile_entry_t& m) {
    archive(make_nvp("id", m.id), make_nvp("name", m.name));
};

template<class Archive>
void serialize(Archive& archive, guictrlayout_snapshot_t& m) {
    archive(m.label, m.type, m.activePosition, m.ctrLayout, m.entries, m.splitterPositions);
}
template<class Archive>
void serialize(Archive& archive, guictrlayout_entry_snapshot_t& m) {
    archive(m.label, m.type);
}
template<class Archive>
void serialize(Archive& archive, dawview_layout_t& m) {
    archive(m.left, m.right, m.splitterPositions);
}

template<class Archive>
void load(Archive& archive, project_file& file, const std::uint32_t version) {
    file.fileFmtVersion = version;
    if (version < 2)
        return;
    archive(
        make_nvp("projectdata", file.project),
        make_nvp("layout", file.layout),
        make_nvp("samples", file.sampleFileIndex)
    );
    if (version >= 3) {
        archive(make_nvp("layouts", file.layouts));
    }
}

template<class Archive>
void save(Archive& archive, project_file const& file, const std::uint32_t version) {
    archive(
        make_nvp("projectdata", file.project),
        make_nvp("layout", file.layout),
        make_nvp("samples", file.sampleFileIndex),
        make_nvp("layouts", file.layouts)
    );
}

CEREAL_REGISTER_TYPE(guictrlayout_snapshot_t);
CEREAL_REGISTER_POLYMORPHIC_RELATION(guictrlayout_entry_snapshot_t, guictrlayout_snapshot_t)
CEREAL_CLASS_VERSION(plugin_snapshot_t, 12);
CEREAL_CLASS_VERSION(track_snapshot_t, 4);
CEREAL_CLASS_VERSION(automatable_param_ref_t, 1);
CEREAL_CLASS_VERSION(track_layout_snapshot_t, 1);
CEREAL_CLASS_VERSION(project_file, FILE_FORMAT_VERSION);


bool saveDawViewLayoutSnapshot(dawview_layout_t& snapshot, const String& path) {
    using namespace cereal;
    try {
        Stringstream sstream;
        {
            JSONOutputArchive ar(sstream);
            ar(make_nvp("layout", snapshot));
        }
        sstream.flush();
        writeStringStream(App::Platform::toUserdataPath(path), sstream);
        return true;
    } catch (const FileIOException& e) {
        log_printf("savePluginSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("savePluginSnapshot exception: %s\n", e.what());
    }
    return false;
}

std::shared_ptr<dawview_layout_t> loadDawViewLayoutSnapshot(const String& path) {
    using namespace cereal;
    try {
        std::vector<uint8_t> vec;
        ReadFileVector(App::Platform::toUserdataPath(path), vec);
        Stringstream sstream(std::string(vec.cbegin(), vec.cend()));
        std::shared_ptr<dawview_layout_t> snapshot = std::make_shared<dawview_layout_t>();
        dawview_layout_t& ref = *snapshot.get();
        {
            JSONInputArchive ar(sstream);
            ar(make_nvp("layout", ref));
        }
        return snapshot;
    } catch (const FileIOException& e) {
        log_printf("loadDawViewLayoutSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("loadDawViewLayoutSnapshot exception: %s\n", e.what());
    }
    return nullptr;
}

/**
 * @param projectfile
 * @return true if project file is valid
 */
bool validateProjectFile(const std::shared_ptr<project_file>& projectfile) {
    auto trackArr = {
        std::cref(projectfile->project.trackCtr), 
        std::cref(projectfile->project.trackReturnCtr), 
        std::cref(projectfile->project.trackMasterCtr)
    };
    for (const trackcontainer_snapshot_t& trackcontainersnapshot : trackArr) {
        std::vector<int32_t> vec;
        vec.reserve(128);
        for (const track_snapshot_t& tracksnapshot : trackcontainersnapshot.tracks) {
            for (const plugin_snapshot_t& pluginsnapshot : tracksnapshot.data.pluginSnapshots) {
                int32_t globalId = pluginsnapshot.projectGlobalId;
                if (STL_CONTAINS(vec, globalId)) {
                    log_lf(Log::L_WARN, "invalid project: duplicate plugin global id %d found\n", globalId);
                    //return false;
                }
                vec.push_back(globalId);
            }
            for (const clip_t& clip : tracksnapshot.clips) {
                if (clip.notes.hasDuplicates()) {
                    log_lf(Log::L_WARN, "Clip %s on Track %s has duplicate notes\n", StringAsCStr(clip.name), StringAsCStr(tracksnapshot.trackSettings.name));
                }
            }
        }
    }
    return true;
}

std::shared_ptr<project_file> loadProject(const std::vector<uint8_t>& vec) {
    try {
        Stringstream sstream(std::string(vec.begin(), vec.end()));
        auto f = std::make_shared<project_file>();
        {
            JSONInputArchive ar(sstream);
            ar(make_nvp("project", f));
        }
        if (f->fileFmtVersion < 2) {
            log_lf(Log::L_WARN, "legacy project file version %u\n", f->fileFmtVersion);
            return nullptr;
        }
        if (f->project.samplerate == 0) {
            f->project.samplerate = 44100;
            log_lf(Log::L_WARN, "legacy project file without samplerate, setting samplerate to %u\n", 44100);
        }
        if (!validateProjectFile(f)) {
            f.reset();
        }
        return f;
    } catch (const std::exception& e) {
        log_printf("loadProject exception: %s\n", e.what());
    }
    return nullptr;
}

bool saveProject(const std::shared_ptr<project_file>& f, std::vector<uint8_t>& bufferOut) {
    try {
        Stringstream sstream;
        {
            JSONOutputArchive ar(sstream);
            ar(make_nvp("project", f));
        }
        sstream.flush();
        bufferOut.assign(std::istreambuf_iterator<char>(sstream), std::istreambuf_iterator<char>());
        return true;
    } catch (const std::exception& e) {
        log_printf("saveProject exception: %s\n", e.what());
    }
    return false;
}

std::shared_ptr<project_file> loadProjectFromJsonFile(const String& path) {
    try {
        std::vector<uint8_t> vec;
        ReadFileVector(path, vec);
        auto f = loadProject(vec);
        if (f) {
            f->path = path;
        }
        return f;
    } catch (const FileIOException& e) {
        log_printf("loadProject File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    }
    return nullptr;
}

bool saveProjectToJsonFile(const std::shared_ptr<project_file>& f, const String& path) {

    try {
        std::vector<uint8_t> buf;
        buf.reserve(2048);
        if (saveProject(f, buf)) {
            WriteFileVector(path, buf);
            return true;
        }
    } catch (const FileIOException& e) {
        log_printf("saveProject File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("saveProject exception: %s\n", e.what());
    }
    return false;
}

std::shared_ptr<trackcontainer_snapshot_t> loadTrackContainer(const String& path) {
    try {
        std::vector<uint8_t> vec;
        ReadFileVector(path, vec);
        Stringstream sstream(std::string(vec.begin(), vec.end()));
        std::shared_ptr<trackcontainer_snapshot_t> snapshot = std::make_shared<trackcontainer_snapshot_t>();
        {
            JSONInputArchive ar(sstream);
            ar(make_nvp("tracks", *snapshot.get()));
        }
        return snapshot;
    } catch (const FileIOException& e) {
        log_printf("loadTrackContainer File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("loadTrackContainer exception: %s\n", e.what());
    }
    return nullptr;
}

bool saveTrackContainer(const trackcontainer_snapshot_t& container, const String& path) {

    try {
        Stringstream sstream;
        {
            JSONOutputArchive ar(sstream);
            ar(make_nvp("tracks", container));
        }
        sstream.flush();
        writeStringStream(path, sstream);
        return true;
    } catch (const FileIOException& e) {
        log_printf("saveTrackContainer File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("saveTrackContainer exception: %s\n", e.what());
    }
    return false;
}

bool serializePluginSnapshot(const plugin_snapshot_t& snapshot, std::vector<uint8_t>& buf) {
    try {
        Stringstream sstream;
        {
            JSONOutputArchive ar(sstream);
            ar(make_nvp("plugin", snapshot));
        }
        sstream.flush();
        buf.resize(sstream.tellp());
        buf.assign(std::istreambuf_iterator<char>(sstream), std::istreambuf_iterator<char>());
        return true;
    } catch (const std::exception& e) {
        log_printf("savePluginSnapshot exception: %s\n", e.what());
    }
    return false;
}

bool savePluginSnapshot(const plugin_snapshot_t& snapshot, const String& path) {

    try {
        std::vector<uint8_t> buf;
        serializePluginSnapshot(snapshot, buf);
        WriteFileVector(path, buf);
        return true;
    } catch (const FileIOException& e) {
        log_printf("savePluginSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("savePluginSnapshot exception: %s\n", e.what());
    }
    return false;
}

std::shared_ptr<plugin_snapshot_t> deserializePluginSnapshot(std::vector<uint8_t>& vec) {
    try {
        Stringstream sstream(std::string(vec.begin(), vec.end()));
        std::shared_ptr<plugin_snapshot_t> snapshot = std::make_shared<plugin_snapshot_t>();
        {
            JSONInputArchive ar(sstream);
            ar(make_nvp("plugin", *snapshot.get()));
        }
        return snapshot;
    } catch (const std::exception& e) {
        log_printf("loadPluginSnapshot exception: %s\n", e.what());
    }
    return nullptr;
}

std::shared_ptr<plugin_snapshot_t> loadPluginSnapshot(const String& path) {
    try {
        std::vector<uint8_t> vec;
        ReadFileVector(path, vec);
        return deserializePluginSnapshot(vec);
    } catch (const FileIOException& e) {
        log_printf("loadPluginSnapshot File IO exception: %s: %s (%d)\n", e.what(), StringAsCStr(path), e.GetErrorCode());
    } catch (const std::exception& e) {
        log_printf("loadPluginSnapshot exception: %s\n", e.what());
    }
    return nullptr;
}