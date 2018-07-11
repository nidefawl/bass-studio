#include "projectfile.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <cereal/cereal.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/cereal_optional_nvp.hpp>

#include "config.h"
#include "exceptions.h"
#include "seq_time.h"
#include "clip.h"
#include "track.h"
//#include "mainctrl.h"
#include "fileio.h"
#include "layout.h"
#include "project.h"
#include "logging.h"

using namespace cereal;
#define LOG_EX(x) \
	my_printf("Exc while loading/saving: %s\n", x)
template<class Archive>
void serialize(Archive & archive, trackcontainer_snapshot_t & m)
{
	archive(make_nvp("tracklist", m.tracks));
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
	archive(make_nvp("name", m.name), make_nvp("uId", m.uId), make_nvp("slot", m.slot), make_nvp("present", m.present));
	if (version == 1)
	make_optional_nvp(archive, "dataProgram", m.dataChunk2);
	make_optional_nvp(archive, "parameters", m.params);
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
}

template <class Archive>
void save( Archive & archive, plugin_snapshot_t const & m, const std::uint32_t version)
{
	archive(make_nvp("name", m.name), make_nvp("uId", m.uId), make_nvp("slot", m.slot), make_nvp("present", m.present));
	make_optional_nvp(archive, "parameters", m.params);
	make_optional_nvp(archive, "automatedParams", m.automatedParams);
	make_optional_nvp(archive, "globalId", m.projectGlobalId);
	make_optional_nvp(archive, "enabled", m.enabled);
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
}
template<class Archive>
void serialize(Archive & archive, track_params_snapshot_t & m)
{
	archive(make_nvp("params", m.params), make_nvp("automation", m.automatedParams));
}
template<class Archive>
void serialize(Archive & archive, track_impl_snapshot_t & m)
{
	archive(make_nvp("plugins", m.plugins));
	make_optional_nvp(archive, "track", m.trackParams);
}
template<class Archive>
void serialize(Archive & archive, tracksettings_t & m)
{
	archive(make_nvp("name", m.name),
			make_nvp("height", m.height),
			make_nvp("rgb", m.rgb),
			make_nvp("type", m.type));
	make_optional_nvp(archive, "hideAutomation", m.hideAutomation);
	make_optional_nvp(archive, "hideTrack", m.hideTrack);
}
template<class Archive>
void serialize(Archive & archive, track_snapshot_t & m)
{
	make_optional_nvp(archive, "idx", m.localIdx);
	archive(make_nvp("settings", base_class<tracksettings_t>(&m)), make_nvp("clips", m.clips), make_nvp("plugins", m.plugins));
	make_optional_nvp(archive, "automation", m.automationLanes);
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
template<class Archive>
void serialize(Archive & archive, note_t & m)
{
	archive(make_nvp("time", m.time),
			make_nvp("len", m.len),
			make_nvp("pitch", m.pitch),
			make_nvp("enabled", m.enabled));
}
template<class Archive>
void serialize(Archive & archive, project_globals_t & m)
{
	archive(make_nvp("loopEnabled", m.loopEnabled),
			make_nvp("loopStart", m.loopStart),
			make_nvp("loopLen", m.loopLen));
	make_optional_nvp(archive, "tempo100", m.tempo100);
	make_optional_nvp(archive, "loopLen", m.loopLen);
	make_optional_nvp(archive, "signatureNum", m.signatureNum);
	make_optional_nvp(archive, "signatureDenom", m.signatureDenom);
	make_optional_nvp(archive, "playbackPos", m.playbackPos);
	make_optional_nvp(archive, "cursor", m.cursor);
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
	make_optional_nvp(archive, "layout", file.layout);
	archive(cereal::make_nvp("samples", file.sampleFileIndex));
}
CEREAL_CLASS_VERSION( project_file, FILE_FORMAT_VERSION);
CEREAL_CLASS_VERSION( plugin_snapshot_t, 2 );


std::shared_ptr<project_file> loadProjectFile(MainCtrl* ctrl, String& path) {
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
		return f;
	}
	catch (const FileIOException& e)
	{
		my_printf("loadProject File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
		my_printf("loadProject exception: %s\n", e.what());
	}
    return nullptr;
}
bool saveProject(std::shared_ptr<project_file> f, String& path) {

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
		my_printf("saveProject File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
	}
	catch (const std::exception& e)
	{
		my_printf("saveProject exception: %s\n", e.what());
	}
	return false;
}

