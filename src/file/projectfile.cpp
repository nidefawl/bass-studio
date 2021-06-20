#include "projectfile.h"
#include "projectfile-snapshot.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/cereal_optional_nvp.hpp>

#include "config.h"
#include "exceptions.h"
#include "seq_time.h"
#include "str_util.h"
#include "clip.h"
#include "track.h"
#include "track_snapshot.h"
#include "fileio.h"
#include "layout.h"
#include "project.h"
#include "automation.h"
#include "logging.h"

using namespace cereal;
#define LOG_EX(x) \
	log_printf("Exc while loading/saving: %s\n", x)
template<class Archive>
void serialize(Archive & archive, trackcontainer_snapshot_t & m)
{
	archive(make_nvp("tracklist", m.tracks));
	make_optional_nvp(archive, "hierachy", m.hierachy);
}
template<class Archive>
void serialize(Archive & archive, param_snapshot_t & m)
{
	archive(make_nvp("idx", m.idx), make_nvp("val", m.val));
}
template<class Archive>
void serialize(Archive & archive, automationlane_snapshot_t & m)
{
	archive(make_nvp("type", m.type), make_nvp("paramIdx", m.paramIdx));
	make_optional_nvp(archive, "height", m.height);
	make_optional_nvp(archive, "refId", m.refId);
	make_optional_nvp(archive, "subtrackType", m.subtrackType);
}
template<class Archive>
void serialize(Archive & archive, automation_view_t & m)
{
	archive(make_nvp("param", m.targetParam), make_nvp("data", m.points));
	make_optional_nvp(archive, "active", m.active);

}
//template<class Archive>
//void load(Archive & archive, plugin_snapshot_t & m)
//{
//	archive(make_nvp("name", m.name), make_nvp("uId", m.uId), make_nvp("slot", m.slot), make_nvp("present", m.present));
//	make_optional_nvp(archive, "dataProgram", m.dataChunk2);
//	make_optional_nvp(archive, "parameters", m.params);
//	make_optional_nvp(archive, "automatedParams", m.automatedParams);
//	make_optional_nvp(archive, "globalId", m.projectGlobalId);
//	make_optional_nvp(archive, "enabled", m.enabled);
//	make_optional_nvp(archive, "data", m.dataChunk);
//}
template <class Archive>
void load( Archive & archive, plugin_snapshot_t & m, const std::uint32_t version)
{
	if (version > 2) {
		archive(make_nvp("pluginType", m.pluginType), make_nvp("plugins", m.pluginSnapshots));
	}
	archive(make_nvp("name", m.name), make_nvp("uId", m.uId), make_nvp("slot", m.slot), make_nvp("present", m.present));
	if (version == 1)
	make_optional_nvp(archive, "dataProgram", m.dataChunk2);
	if (version < 4) {
		std::vector<param_snapshot_t> allParams;
		std::vector<param_snapshot_t> nonHostParams;
		make_optional_nvp(archive, "parameters", nonHostParams);
		make_optional_nvp(archive, "hostParams", allParams);
		for (auto p : nonHostParams) {
			p.idx += PARAM_OFFSET_EXTERNAL;
			allParams.push_back(p);
		}
		m.params = allParams;
	} else {
		archive(make_nvp("parameters", m.params));
	}
	make_optional_nvp(archive, "automatedParams", m.automatedParams);
	make_optional_nvp(archive, "globalId", m.projectGlobalId);
	make_optional_nvp(archive, "enabled", m.enabled);
	if (version == 1)
	make_optional_nvp(archive, "data", m.dataChunk);

	if (version > 1) {
			{
			size_type size;
			archive(make_nvp("sizeprogramdata", size));
			m.dataChunk2.resize(size);
			((JSONInputArchive*)&archive)->loadBinaryValue((void*)m.dataChunk2.data(), size, "programdata");
			}
			{
			size_type size;
			archive(make_nvp("sizeplugindata", size));
			m.dataChunk.resize(size);
			((JSONInputArchive*)&archive)->loadBinaryValue((void*)m.dataChunk.data(), size, "plugindata");
			}
	}
	if (version > 2) {
		archive(make_nvp("plugins", m.pluginSnapshots));
	}
	if (version > 4) {
		archive(make_nvp("currentProgram", m.currentProgram));
	}
}

template <class Archive>
void save( Archive & archive, plugin_snapshot_t const & m, const std::uint32_t version)
{
	archive(make_nvp("pluginType", m.pluginType));
	archive(make_nvp("name", m.name), make_nvp("uId", m.uId), make_nvp("slot", m.slot), make_nvp("present", m.present));
	archive(make_nvp("parameters", m.params));
	archive(make_nvp("automatedParams", m.automatedParams));
	archive(make_nvp("globalId", m.projectGlobalId));
	archive(make_nvp("enabled", m.enabled));
	{
		size_type size = m.dataChunk2.size();
		archive(make_nvp("sizeprogramdata", size));
		((JSONOutputArchive*)&archive)->saveBinaryValue(m.dataChunk2.data(), size, "programdata");
	}
	{
		size_type size = m.dataChunk.size();
		archive(make_nvp("sizeplugindata", size));
		((JSONOutputArchive*)&archive)->saveBinaryValue(m.dataChunk.data(), size, "plugindata");
	}
	archive(make_nvp("plugins", m.pluginSnapshots));
	archive(make_nvp("currentProgram", m.currentProgram));
}
template<class Archive>
void serialize(Archive & archive, track_params_snapshot_t & m)
{
	archive(make_nvp("params", m.params), make_nvp("automation", m.automatedParams));
}
template<class Archive>
void serialize(Archive & archive, arp_snapshot & m)
{
	archive(make_nvp("params", m.params), make_nvp("automation", m.automatedParams));
}
template<class Archive>
void serialize(Archive & archive, io_configuration_snapshot_t & m)
{
	archive(make_nvp("channelOffset", m.channelOffset),
			make_nvp("externalInputId", m.externalInputId),
			make_nvp("externalInputId", m.externalInputId),
			make_nvp("externalInputType", m.externalInputType),
			make_nvp("inputType", m.inputType),
			make_nvp("stageEndPointType", m.stageEndPointType),
			make_nvp("stageId", m.stageId) );
}
template<class Archive>
void serialize(Archive & archive, track_io_configuration_snapshot_t & m)
{
	archive(make_nvp("input", m.input), make_nvp("output", m.output));
}
template<class Archive>
void serialize(Archive & archive, track_effect_routing_snapshot_t & m)
{
//	archive(make_nvp("inputRoutingOutputStage", m.inputRoutingOutputStage));
	archive(make_nvp("inputRoutingOutputStage", m.inputRoutingOutputStage), make_nvp("inputRoutingEffects", m.inputRoutingEffects), make_nvp("routingState", m.routingState));

}
template<class Archive>
void serialize(Archive & archive, track_impl_snapshot_t & m)
{
	archive(make_nvp("plugins", m.pluginSnapshots));
	make_optional_nvp(archive, "track", m.trackParams);
	make_optional_nvp(archive, "arp", m.trackArp);
	make_optional_nvp(archive, "io", m.trackIO);
	make_optional_nvp(archive, "routing", m.effectRouting);
}
template<class Archive>
void save(Archive & archive, tracksettings_t const & m, const std::uint32_t version)
{
	archive(make_nvp("name", m.name),
			make_nvp("rgb", m.rgb),
			make_nvp("type", m.type));
}
template<class Archive>
void load(Archive & archive, tracksettings_t & m, const std::uint32_t version)
{
	archive(make_nvp("name", m.name),
			make_nvp("rgb", m.rgb),
			make_nvp("type", m.type));
}
template<class Archive>
void serialize(Archive & archive, tracklayout_settings_t & m)
{
	archive(make_nvp("height", m.height),
			make_nvp("hideSubtracks", m.hideSubtracks),
			make_nvp("hideTrack", m.hideTrack));
}
template<class Archive>
void serialize(Archive & archive, track_layout_snapshot_t & m)
{
//	archive(make_nvp("layout", m.layout),
//			make_nvp("subtracks", m.automationLanes));
}
template<class Archive>
void serialize(Archive & archive, track_id_snapshot_t & m)
{
	archive(make_nvp("stageId", m.stageId),
			make_nvp("inputStageId", m.inputStageId),
			make_nvp("outputStageId", m.outputStageId),
			make_nvp("outputPostStageId", m.outputPostStageId));
}
//template<class Archive>
//void serialize(Archive & archive, track_snapshot_t & m)
//{
//			make_optional_nvp(archive, "idx", m.localIdx);
//			archive(make_nvp("settings", base_class<tracksettings_t>(&m)), make_nvp("clips", m.clips), make_nvp("plugins", m.plugins));
//	//		int32_t stageId = m.stageIds.stageId;
//	//	    archive(make_nvp("stageId", stageId));
//	//	    stageId*=4;
//	//	    m.stageIds.stageId = stageId++;
//	//	    m.stageIds.inputStageId = stageId++;
//	//	    m.stageIds.outputStageId = stageId++;
//	//	    m.stageIds.outputPostStageId = stageId++;
//				    archive(make_nvp("stageIds", m.stageIds));
//}
template<class Archive>
void load(Archive & archive, track_snapshot_t & m, const std::uint32_t version)
{
    //	make_optional_nvp(archive, "automation", m.layouts);
    //make_optional_nvp(archive, "stageId", m.stageId);
	if (version == 0) {
		make_optional_nvp(archive, "idx", m.localIdx);
		archive(make_nvp("settings", base_class<tracksettings_t>(&m)), make_nvp("clips", m.clips), make_nvp("plugins", m.plugins));
		int32_t stageId = m.stageIds.stageId;
	    archive(make_nvp("stageId", stageId));
	    stageId*=4;
	    m.stageIds.stageId = stageId++;
	    m.stageIds.inputStageId = stageId++;
	    m.stageIds.outputStageId = stageId++;
	    m.stageIds.outputPostStageId = stageId++;
	} else {
		archive(make_nvp("idx", m.localIdx));
		archive(make_nvp("settings", base_class<tracksettings_t>(&m)), make_nvp("clips", m.clips), make_nvp("plugins", m.plugins));
	    archive(make_nvp("stageIds", m.stageIds));
	}
}
template<class Archive>
void save(Archive & archive, const track_snapshot_t & m, const std::uint32_t version)
{
	make_optional_nvp(archive, "idx", m.localIdx);
	archive(make_nvp("settings", base_class<tracksettings_t>(&m)), make_nvp("clips", m.clips), make_nvp("plugins", m.plugins));
    archive(make_nvp("stageIds", m.stageIds));
}
template<class Archive>
void serialize(Archive & archive, layout_grid_t & m)
{
	archive(make_nvp("offset", m.offset),
			make_nvp("zoom", m.zoom));
}
template<class Archive>
void serialize(Archive & archive, layout_pianoroll_t & m)
{
	archive(make_nvp("offset", m.yoffset),
			make_nvp("scale", m.yscale));
	make_optional_nvp(archive, "fold", m.fold);
}
template<class Archive>
void serialize(Archive & archive, clip_editor_layout_t & m)
{
	archive(make_nvp("layoutGrid", m.layoutGrid),
			make_nvp("layoutPianoRoll", m.layoutPianoRoll));
}
template<class Archive>
void serialize(Archive & archive, automation_point_t & m)
{

	archive(make_nvp("time", m.time),
			make_nvp("val", m.val)
			);
}
template<class Archive>
void serialize(Archive & archive, clip_t & m)
{
	archive(make_nvp("name", m.name),
			make_nvp("time", m.time),
			make_nvp("len", m.len),
			make_nvp("offsetStart", m.offsetStart),
			make_nvp("loopLen", m.loopLen),
			make_nvp("enabled", m.enabled),
			make_nvp("rgb", m.rgb));
	make_optional_nvp(archive, "loopStart", m.loopStart);
	make_optional_nvp(archive, "loopEnabled", m.loopEnabled);
	make_optional_nvp(archive, "noLayout", m.noLayout);
	make_optional_nvp(archive, "editorLayout", m.editorLayout);
	make_optional_nvp(archive, "clip_notes", m.notes);
	make_optional_nvp(archive, "clip_audio", m.audio);
	make_optional_nvp(archive, "type", m.clipType);
	make_optional_nvp(archive, "offsetSamples", m.offsetSamples);
	make_optional_nvp(archive, "lenSamples", m.lenSamples);
	if (m.loopLen == 0) {
		m.loopStart = m.offsetStart;
		m.loopLen = m.len;
	}
}

template<class Archive>
void save(Archive & archive, clip_notes_t const & m)
{
	archive(make_nvp("notes", m.m_list));
}
template<class Archive>
void load(Archive & archive, clip_notes_t & m)
{
	archive(make_nvp("notes", m.m_list));
	m.updateBounds();
}
template<class Archive>
void save(Archive & archive, clip_audio_t const & m)
{
	make_optional_nvp(archive, "id", m.id);
}
template<class Archive>
void load(Archive & archive, clip_audio_t & m)
{
	make_optional_nvp(archive, "id", m.id);
}
//TODO: don't archive each note seperately
template<class Archive>
void save(Archive & archive, note_t const & m)
{
	archive(make_nvp("time", m.time),
			make_nvp("len", m.len),
			make_nvp("pitch", m.pitch),
			make_nvp("flags", m.flags));
	float fVel = m.velocity;
	archive(make_nvp("velocity", fVel));
}
template<class Archive>
void load(Archive & archive, note_t & m)
{
	archive(make_nvp("time", m.time),
			make_nvp("len", m.len),
			make_nvp("pitch", m.pitch));
	//handle old format, pre 2019/06/04
	if (!make_optional_nvp(archive, "flags", m.flags)) {
		bool b = true;
		make_optional_nvp(archive, "enabled", b);
		m.flags = b ? NoteFlags::ENABLED : 0;
	}
	float fVel = 0;
	if (make_optional_nvp(archive, "velocity", fVel)) {
		m.velocity = CLAMP_I(static_cast<int32_t>(fVel), 0, 127);
	}
}
template<class Archive>
void serialize(Archive & archive, project_globals_t & m)
{
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
void serialize(Archive & archive, project_snapshot_t & m)
{
	archive(make_nvp("masterTracks", m.trackMasterCtr),
			make_nvp("returnTracks", m.trackReturnCtr),
			make_nvp("tracks", m.trackCtr));
	make_optional_nvp(archive, "globals", m.globals);
};
template<class Archive>
void serialize(Archive & archive, project_layout_t & m)
{
	archive(make_nvp("grid", m.layoutGrid),
			make_nvp("scrollOffsetX", m.scrollOffsetX));
};

namespace DAW {
template<class Archive>
void serialize(Archive & archive, Cursor & m)
{
	archive(make_nvp("pos", m.cursorPos),
			make_nvp("track", m.cursorTrack),
			make_nvp("subtrack", m.cursorSubTrack),
			make_nvp("range", m.selRange),
			make_nvp("trackrange", m.selTrackRange),
			make_nvp("subtrackrange", m.selSubTrackRange));
};
}

template<class Archive>
void serialize(Archive & archive, samplefile_index_t & m)
{
	archive(make_nvp("list", m.list));
};
template<class Archive>
void serialize(Archive & archive, samplefile_entry_t & m)
{
	archive(make_nvp("id", m.id), make_nvp("name", m.name));
};

template <class Archive>
void load( Archive & archive, project_file & file, const std::uint32_t version)
{
	if (version != FILE_FORMAT_VERSION)
		return;
	archive(cereal::make_nvp("projectdata", file.project));
	make_optional_nvp(archive, "layout", file.layout);
	make_optional_nvp(archive, "samples", file.sampleFileIndex);
}

template <class Archive>
void save( Archive & archive, project_file const & file, const std::uint32_t version)
{
	archive(cereal::make_nvp("projectdata", file.project));
	archive(cereal::make_nvp("layout", file.layout));
	archive(cereal::make_nvp("samples", file.sampleFileIndex));
}
CEREAL_CLASS_VERSION( project_file, FILE_FORMAT_VERSION);
CEREAL_CLASS_VERSION( plugin_snapshot_t, 5 );
CEREAL_CLASS_VERSION( track_snapshot_t, 1 );

/**
 * @param projectfile
 * @return true if project file is valid
 */
bool validateProjectFile(std::shared_ptr<project_file> projectfile) {
	auto trackArr = {projectfile->project.trackCtr, projectfile->project.trackReturnCtr, projectfile->project.trackMasterCtr};
	for (const trackcontainer_snapshot_t& trackcontainersnapshot : trackArr) {
		std::vector<int32_t> vec;
		vec.reserve(128);
		for (const track_snapshot_t& tracksnapshot : trackcontainersnapshot.tracks) {
			for (const plugin_snapshot_t& pluginsnapshot : tracksnapshot.plugins.pluginSnapshots) {
				int32_t globalId = pluginsnapshot.projectGlobalId;
				if (std::binary_search(vec.begin(), vec.end(), globalId)) {
					log_printf("invalid project: duplicate plugin global id %d found\n", globalId);
//					return false;
				}
				vec.push_back(globalId);
			}

		}
	}
	return true;
}
std::shared_ptr<project_file> loadProjectFile(String& path) {
	try {
		std::vector<uint8_t> vec;
		ReadFileVector(path, vec);

		Stringstream sstream(std::string(vec.begin(), vec.end()));
		std::shared_ptr<project_file> f = std::make_shared<project_file>();
		{
			JSONInputArchive ar(sstream);
			ar(make_nvp("project", f));
		}
		f->path = path;
		if (!validateProjectFile(f)) {
			f.reset();
		}
		return f;
	}
	catch (const FileIOException& e)
	{
		log_printf("loadProject File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
		log_printf("loadProject exception: %s\n", e.what());
	}
    return nullptr;
}
bool saveProject(std::shared_ptr<project_file> f, const String& path) {

	try {
		Stringstream sstream;
		{
			JSONOutputArchive ar(sstream);
			ar(make_nvp("project", f));
		}
		sstream.flush();
		Stringstream::pos_type len = sstream.tellp();
		std::vector<uint8_t> buf(len);
		buf.assign(std::istreambuf_iterator<char>(sstream), std::istreambuf_iterator<char>());
		WriteFileVector(path, buf);
		return true;
	}
	catch (const FileIOException& e)
	{
		log_printf("saveProject File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
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
	}
	catch (const FileIOException& e)
	{
		log_printf("loadTrackContainer File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
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
	}
	catch (const FileIOException& e)
	{
		log_printf("saveTrackContainer File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
		log_printf("saveTrackContainer exception: %s\n", e.what());
	}
	return false;
}

bool savePluginSnapshot(const plugin_snapshot_t& snapshot, const String& path) {

	try {
		Stringstream sstream;
		{
			JSONOutputArchive ar(sstream);
			ar(make_nvp("plugin", snapshot));
		}
		sstream.flush();
		writeStringStream(path, sstream);
		return true;
	}
	catch (const FileIOException& e)
	{
		log_printf("savePluginSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
		log_printf("savePluginSnapshot exception: %s\n", e.what());
	}
	return false;
}

std::shared_ptr<plugin_snapshot_t> loadPluginSnapshot(const String& path) {
	try {
		std::vector<uint8_t> vec;
		ReadFileVector(path, vec);
		Stringstream sstream(std::string(vec.begin(), vec.end()));
		std::shared_ptr<plugin_snapshot_t> snapshot = std::make_shared<plugin_snapshot_t>();
		{
			JSONInputArchive ar(sstream);
			ar(make_nvp("plugin", *snapshot.get()));
		}
		return snapshot;
	}
	catch (const FileIOException& e)
	{
		log_printf("loadPluginSnapshot File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
		log_printf("loadPluginSnapshot exception: %s\n", e.what());
	}
    return nullptr;
}
