#include "projectfile.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <memory>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/cereal_optional_nvp.hpp>

#include "config.h"
#include "exceptions.h"
#include "seq_time.h"
#include "clip.h"
#include "track.h"
#include "mainctrl.h"
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

}
template<class Archive>
void serialize(Archive & archive, plugin_snapshot_t & m)
{
	archive(make_nvp("name", m.name), make_nvp("uId", m.uId), make_nvp("slot", m.slot), make_nvp("present", m.present));
	make_optional_nvp(archive, "data", m.dataChunk);
	make_optional_nvp(archive, "dataProgram", m.dataChunk2);
	make_optional_nvp(archive, "parameters", m.params);
	make_optional_nvp(archive, "automatedParams", m.automatedParams);
	make_optional_nvp(archive, "globalId", m.projectGlobalId);
}
template<class Archive>
void serialize(Archive & archive, track_plugins_snapshot_t & m)
{
	archive(make_nvp("plugins", m.plugins));
	make_optional_nvp(archive, "gain", m.gain);
}
template<class Archive>
void serialize(Archive & archive, tracksettings_t & m)
{
	archive(make_nvp("idx", m.idx),
			make_nvp("name", m.name),
			make_nvp("height", m.height),
			make_nvp("rgb", m.rgb),
			make_nvp("enabled", m.enabled),
			make_nvp("type", m.type));
	make_optional_nvp(archive, "hideAutomation", m.hideAutomation);
	make_optional_nvp(archive, "hideTrack", m.hideTrack);
}
template<class Archive>
void serialize(Archive & archive, track_snapshot_t & m)
{
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
			make_nvp("rgb", m.rgb),
			make_nvp("clip_notes", m.notes));
	make_optional_nvp(archive, "loopStart", m.loopStart);
	make_optional_nvp(archive, "loopEnabled", m.loopEnabled);
	make_optional_nvp(archive, "noLayout", m.noLayout);
	make_optional_nvp(archive, "editorLayout", m.editorLayout);
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
}
template<class Archive>
void serialize(Archive & archive, project_snapshot_t & m)
{
	archive(make_nvp("masterTracks", m.trackMasterCtr),
			make_nvp("returnTracks", m.trackReturnCtr),
			make_nvp("tracks", m.trackCtr));
	make_optional_nvp(archive, "globals", m.globals);
};

template <class Archive>
void load( Archive & archive, project_file & file, const std::uint32_t version)
{
	if (version != FILE_FORMAT_VERSION)
		return;
	archive(cereal::make_nvp("projectdata", file.project));
}

template <class Archive>
void save( Archive & archive, project_file const & file, const std::uint32_t version)
{
	archive(cereal::make_nvp("projectdata", file.project));
}
CEREAL_CLASS_VERSION( project_file, FILE_FORMAT_VERSION);

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

