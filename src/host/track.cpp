#include <algorithm>


#include "logging.h"
#include "samplerate.h"
#include "seq_util.h"
#include "seq_time.h"
#include "seq_math.h"


#include "../gui/pluginctr.h"
#include "../gui/trackctr.h"
#include "../gui/trackcontrols.h"
#include "../gui/trackcontent.h"

#include "clip.h"
#include "track.h"
#include "track_audiodata.h"

#include "vst_plugin.h"
#include "vst_plugin_handles.h"
#include "vst_host.h"

#include "mainctrl.h"

#include "leak_detect.h"

const tick_t INVALID_TICK = 1 << 31;

#define ERROR_LOG(x) (my_printf("ERROR: %s\n", x))

void deleteClip(clip_t* cl, delete_cb *cb) {
	if (cb)
		cb->preClipDelete(cl);
	gui_clip* gClip = cl->gClip;
	if (gClip) {
		gClip->m_track->content->remove(gClip);
		DELETE_PTR(gClip);
	}
	delete cl;
}
void deleteTrack(track_t* tr, delete_cb *cb) {
	if (cb)
		cb->preTrackDelete(tr);
	if (tr->type == TRACK_TYPE_MIDI) {
		trackdata_midi_t& midi = tr->getMidi();
		std::vector<clip_t*>& clips = midi.clips;
		for (auto itClip = clips.begin(); itClip != clips.end(); itClip++) {
			clip_t* cl = *itClip;
			deleteClip(cl, cb);
		}
		clips.clear();
	}
	if (tr->mixer) {
		delete (tr->mixer);
	}
	if (tr->content) {
		delete (tr->content);
	}
	if (tr->audio) {
		delete (tr->audio);
	}
	my_printf("DELETE TRACK %08X\n", (uint64_t) tr);
	delete tr;
}

std::vector<clip_t*>::iterator trackdata_midi_t::removeClip(clip_t* clip) {
	auto it = std::find(clips.begin(), clips.end(), clip);
	if (it == clips.end()) {
		throw applogicexception("track - attempt to remove non-present clip");
	}
	return clips.erase(it);
}
std::pair<clip_t*, clip_t*> trackdata_midi_t::getMinMax() {
	auto minmax = std::minmax_element(clips.begin(), clips.end(),
        [] (clip_t* const& lhs, clip_t* const& rhs) {
		return lhs->time < rhs->time;
	});
	std::pair<clip_t*, clip_t*> pairPtr;
    if (minmax.first != clips.end()) {
    	pairPtr.first = *minmax.first;
    }
    if (minmax.second != clips.end()) {
    	pairPtr.second = *minmax.second;
    }
    return pairPtr;
}
tick_t trackdata_midi_t::start() {
	auto minmax = getMinMax();
	return (minmax.first?minmax.first->time:0);
}
tick_t trackdata_midi_t::end() {
	auto minmax = getMinMax();
	return (minmax.second?(minmax.second->time+minmax.second->len):0);
}

trackbasecontainer_t::~trackbasecontainer_t() {
	for (auto it = tracks.begin(); it != tracks.end(); it++) {
		track_t* tr = *it;
		deleteTrack(tr, NULL);
	}
	tracks.clear();
}
void trackallcontainer_t::addTrack(int trackInsertPos, track_t* newTrack) {
	int numTracks = (int) this->tracks.size();
	auto it = std::find(tracks.begin(), tracks.end(), newTrack);
	if (it != tracks.end()) {
		assert(0);
		throw applogicexception("attempt to add track twice");
	}
	if (trackInsertPos < 0 || trackInsertPos >= numTracks) {
		tracks.push_back(newTrack);
	} else {
		tracks.insert(tracks.begin() + trackInsertPos, newTrack);
	}
	vsthost* host = vsthost::getInstance();
	newTrack->audio = host->createAudio(newTrack);
	tracksubcontainer_t* subCtr = trackTypeCtrs[newTrack->type];
	track_vector& vec = subCtr->tracks;
	vec.push_back(newTrack);
	int32_t idx = 0;
	for (track_t* t : tracks) {
		t->idx = idx++;
	}
	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);
}
void trackallcontainer_t::removeTrack(track_t* track) {
	if (!removeEntry(tracks, track)) {
		assert(0);
		throw applogicexception("trackcontainer_t - attempt to remove non-present element");
	}
	tracksubcontainer_t* subCtr = trackTypeCtrs[track->type];
	track_vector& vec = subCtr->tracks;
	removeEntry(vec, track);
	int32_t idx = 0;
	for (track_t* t : tracks) {
		t->idx = idx++;
	}
	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);
}

void trackallcontainer_t::copyTo(project_snapshot_t& project) {
	for (track_t* t : *this) {
		my_printf("TRACK[%d] = %s\n", t->idx, StringAsCStr(t->name));
	}
	trackCtr.copyTo(project.trackCtr);
	trackMasterCtr.copyTo(project.trackMasterCtr);
	trackReturnCtr.copyTo(project.trackReturnCtr);
}
void trackallcontainer_t::copyFrom(project_snapshot_t& project) {
	my_printf("project.tracks: midi: %d returN: %d master: %d\n",
			project.trackCtr.tracks.size(),
			project.trackReturnCtr.tracks.size(),
			project.trackMasterCtr.tracks.size());
	assert(tracks.empty());
	trackCtr.copyFrom(project.trackCtr);
	assert(trackCtr.size()==project.trackCtr.tracks.size());

	trackReturnCtr.copyFrom(project.trackReturnCtr);
	assert(trackReturnCtr.size()==project.trackReturnCtr.tracks.size());

	trackMasterCtr.copyFrom(project.trackMasterCtr);
	assert(trackMasterCtr.size()==project.trackMasterCtr.tracks.size());


	addAll(tracks, trackCtr.tracks);
	addAll(tracks, trackReturnCtr.tracks);
	addAll(tracks, trackMasterCtr.tracks);
	int32_t idx = 0;
	for (track_t* track : tracks) {
		track->idx = idx++;
	}
	assert(tracks.size()==(project.trackCtr.tracks.size()+project.trackMasterCtr.tracks.size()+project.trackReturnCtr.tracks.size()));

	std::sort(tracks.begin(), tracks.end(), [](track_t* const & a, track_t* const & b) {
		return a->idx < b->idx;
	});
	tracksBottom.tracks.clear();
	addAll(tracksBottom.tracks, trackReturnCtr.tracks);
	addAll(tracksBottom.tracks, trackMasterCtr.tracks);
	vsthost* host = vsthost::getInstance();
	for (track_t* t : tracks) {
		t->audio = host->createAudio(t);
	}
}
void trackallcontainer_t::loadPlugins(project_snapshot_t& project) {
	trackCtr.loadPlugins(this, project.trackCtr);
	trackReturnCtr.loadPlugins(this, project.trackReturnCtr);
	trackMasterCtr.loadPlugins(this, project.trackMasterCtr);
}
void trackallcontainer_t::copyTracks(int32_t trackBegin, int32_t trackEnd, trackstate_t& _out) {
	_out.reset();
	for (track_t* t: tracks) {
		if (t->idx >= trackBegin && t->idx <= trackEnd) {
			my_printf("copy track %d\n", t->idx);
			track_snapshot_t* trackCopy = new track_snapshot_t(t, false);
			_out.tracks.push_back(trackCopy);
		} else {

			my_printf("NOT copy track %d\n", t->idx);
		}
	}
}
track_t &track_t::operator =(const track_snapshot_t &obj) {
	std::vector<clip_t*>& clips = midi.clips;
	clips.clear();
	for (const clip_t& clip : obj.clips) {
		clips.push_back(new clip_t(clip));
	}
	midi.sortClips();
	idx = obj.idx;
	name = obj.name;
	enabled = obj.enabled;
	type = obj.type;
	height = obj.height;
	rgb = obj.rgb;
	scrolloffset = 0;
	return *this;
}
track_t::track_t(const track_snapshot_t &a) : tracksettings_t(a) {
	std::vector<clip_t*>& clips = midi.clips;
	for (const clip_t& clip : a.clips) {
		clip_t* clipInstance = new clip_t(clip);
		clips.push_back(clipInstance);
	}
//	automation.points = a.points;
	assert(this->mixer == NULL);
	assert(this->content == NULL);
}
void createSnapshot(plugin_snapshot_t& ps, vstplugin* plugin, bool storePluginChunks) {
	ps.present = true;
	ps.slot = 0;
	ps.projectGlobalId = plugin->projectGlobalId;
	ps.uId = plugin->uId;
	ps.name = plugin->sName;
	if (storePluginChunks) {
		void* pluginData;
		int32_t pluginDataSize = plugin->dispatch(effGetChunk, 0, 0, &pluginData, 0);
		if (pluginDataSize > 0 && pluginData) {
			uint8_t* ptrData = reinterpret_cast<uint8_t*>(pluginData);
			ps.dataChunk.reserve(pluginDataSize);
			ps.dataChunk.assign(ptrData, ptrData + pluginDataSize);
			my_printf("Plugin %s: Save data1[%d]\n", StringAsCStr(plugin->sName), pluginDataSize);

		}
		void* pluginData2;
		int32_t pluginDataSize2 = plugin->dispatch(effGetChunk, 1, 0, &pluginData2, 0);
		if (pluginDataSize2 > 0 && pluginData2) {
			uint8_t* ptrData = reinterpret_cast<uint8_t*>(pluginData2);
			ps.dataChunk2.reserve(pluginDataSize2);
			ps.dataChunk2.assign(ptrData, ptrData + pluginDataSize2);
			my_printf("Plugin %s: Save data2[%d]\n", StringAsCStr(plugin->sName), pluginDataSize2);
		}
	}
	ps.params.reserve(plugin->params.size());
	for (vst_param& param : plugin->params) {
		float val = plugin->getParamValue(param.idx);
		param_snapshot_t t{param.idx, val};
		ps.params.push_back(t);
	}
	for (automated_param_t& automatedParam : plugin->automatedParams) {
		assert(automatedParam.src);
		if (automatedParam.src->points.empty()) {
			continue;
		}
		automation_view_t automation;
		automation.targetParam = automatedParam.paramIdx;
		automation.points = automatedParam.src->points;
		ps.automatedParams.push_back(automation);
	}
}
track_plugins_snapshot_t::track_plugins_snapshot_t(const track_t &a, bool storePluginChunks) {
	track_plugins_t* p = a.audio;
	if (p) {
		gain = p->mixer.gain;
		int32_t nPlugins = p->effects.size();
		if (p->instrument) nPlugins++;
		plugins.reserve(nPlugins);
		if (p->instrument) {
			plugin_snapshot_t ps;
			createSnapshot(ps, p->instrument, storePluginChunks);
			ps.slot = p->instrument->handle->slot;
			this->plugins.push_back(std::move(ps));
		}
		for (vstplugin* effect : p->effects) {
			plugin_snapshot_t ps;
			createSnapshot(ps, effect, storePluginChunks);
			ps.slot = effect->handle->slot;
			this->plugins.push_back(std::move(ps));
		}
	}
}
track_snapshot_t::track_snapshot_t(track_t* track, bool storePluginChunks)
  : tracksettings_t(*track), plugins(*track, storePluginChunks)
{
	std::vector<clip_t*>& otherClips = track->getMidi().clips;
	for (clip_t* clip : otherClips) {
		clips.emplace_back(*clip);
	}
	track_plugins_t* p = track->audio;
	if (p) {
		p->saveAutomationLanes(automationLanes);
	}
}

void tracksubcontainer_t::copyTo(trackcontainer_snapshot_t& out) {
	out.tracks.reserve(tracks.size());
	int32_t idx = 0;
	for (track_t* track : tracks) {
		track_snapshot_t trackCopy(track, true);
		out.tracks.push_back(std::move(trackCopy));
		trackCopy.idx = idx++;
	}
}
void tracksubcontainer_t::copyFrom(trackcontainer_snapshot_t& in) {
	assert(tracks.empty());
	for (track_snapshot_t& trackStatic : in.tracks) {
		track_t* trackCopy = new track_t(trackStatic);
		trackStatic.trackLoaded = trackCopy;
		this->tracks.push_back(trackCopy);
	}
	std::sort(tracks.begin(), tracks.end(), [](track_t* const & a, track_t* const & b) {
		return a->idx < b->idx;
	});
}
void tracksubcontainer_t::loadPlugins(trackallcontainer_t* all, trackcontainer_snapshot_t& in) {
	for (track_snapshot_t& trackStatic : in.tracks) {
		track_t* trackLoaded = trackStatic.trackLoaded;
		assert(trackLoaded->audio);
		vsthost* host = vsthost::getInstance();
//		trackLoaded->audio = host->createAudio(trackLoaded);
		String path;
		const track_plugins_snapshot_t& trackPlugins = trackStatic.plugins;
		trackLoaded->audio->mixer.gain = trackPlugins.gain;
		const std::vector<plugin_snapshot_t>& trPluginList = trackPlugins.plugins;
		for (const plugin_snapshot_t& pluginSnapshot : trPluginList) {
			if (MainCtrl::get()->plugindb.resolve(pluginSnapshot.name, pluginSnapshot.uId, &path)) {
				vstpluginloadres res = host->loadPlugin(path, pluginSnapshot.projectGlobalId);
				if (res.result==0&&res.plugin) {
					vstplugin* plugin = res.plugin;
					if (pluginSnapshot.dataChunk.size() > 0) {
						my_printf("Plugin %s: Load data1[%d]\n", StringAsCStr(res.plugin->sName), pluginSnapshot.dataChunk.size());
						plugin->dispatch(effSetChunk, 0, pluginSnapshot.dataChunk.size(), (void*)pluginSnapshot.dataChunk.data());
					}
					if (pluginSnapshot.dataChunk2.size() > 0) {
						my_printf("Plugin %s: Load data2[%d]\n", StringAsCStr(res.plugin->sName), pluginSnapshot.dataChunk2.size());
						plugin->dispatch(effSetChunk, 1, pluginSnapshot.dataChunk2.size(), (void*)pluginSnapshot.dataChunk2.data());
					}

					const std::vector<param_snapshot_t>& pluginSnapshotParams = pluginSnapshot.params;
					for (const param_snapshot_t& param : pluginSnapshotParams) {
						if (plugin->getParam(param.idx)) {
							plugin->setParamValue(param.idx, param.val);
						}
					}
					host->insertNewPlugin(trackLoaded->audio, plugin, pluginSnapshot.slot);

					const std::vector<automation_view_t>& automatedParams = pluginSnapshot.automatedParams;
					for (const automation_view_t& automatedParam : automatedParams) {
						if (plugin->getParam(automatedParam.targetParam)) {
							automation_t* autom = plugin->getAutomation(automatedParam.targetParam);
							autom->points = automatedParam.points;
						}
					}
				}
			}
		}
		const std::vector<automationlane_snapshot_t>& atl = trackStatic.automationLanes;
		trackLoaded->subtracks.clear();
		bool showSubtracks = !trackLoaded->hideAutomation && !trackLoaded->hideTrack;
		if (!showSubtracks) {
			trackLoaded->audio->atl = atl;
		} else {
			trackLoaded->audio->atl.clear();
			trackLoaded->audio->loadAutomationLanes(atl);
		}
	}
}

void track_t::loadPluginSnapshot(track_snapshot_t& trackStatic) {
	assert(audio);
	const track_plugins_snapshot_t& trackPlugins = trackStatic.plugins;
	audio->mixer.gain = trackPlugins.gain;
	const std::vector<plugin_snapshot_t>& trPluginList = trackPlugins.plugins;
	for (const plugin_snapshot_t& pluginSnapshot : trPluginList) {
		vstplugin* plugin = audio->getPluginById(pluginSnapshot.projectGlobalId);
		if (plugin) {
			const std::vector<param_snapshot_t>& pluginSnapshotParams = pluginSnapshot.params;
			for (const param_snapshot_t& param : pluginSnapshotParams) {
				if (plugin->getParam(param.idx)) {
					plugin->setParamValue(param.idx, param.val);
				}
			}
			const std::vector<automation_view_t>& automatedParams = pluginSnapshot.automatedParams;
			for (const automation_view_t& automatedParam : automatedParams) {
				if (plugin->getParam(automatedParam.targetParam)) {
					automation_t* autom = plugin->getAutomation(automatedParam.targetParam);
					autom->points = automatedParam.points;
				}
			}
		}
	}

}
void track_t::releaseTrackContent() {
	for (clip_t* clip : midi.clips) {
		gui_clip* gClip = clip->gClip;
		if (gClip) {
			if (this->content) {
				this->content->remove(gClip);
			}
			DELETE_PTR(gClip);
		}
	}
}
void trackdata_midi_t::deleteEmptyClips() {
	std::vector<clip_t*>::iterator it = clips.begin();
	while (it != clips.end()) {
		clip_t* c = *it;
		if (c->len <= 0) {
			it = removeClip(c);
			deleteClip(c, MainCtrl::get());
		} else {
			it++;
		}
	}
	sortClips();
}
void trackdata_midi_t::getNotesInRange(tick_t start, tick_t end, tick_t cutStart, tick_t cutEnd, std::vector<note_t>& notes) {
//	my_printf("range %d to %d\n", start, end);
	for (clip_t* clip : clips) {
		if (clip->end() <= start || clip->start() > end) {
			continue;
		}
		clip->getInTimeRange(start, end, cutStart, cutEnd, notes);
	}

}

vstplugin* track_plugins_t::getPluginSlot(int32_t idx) {
	if (idx == 0) {
		return instrument;
	}
	for (vstplugin* effect : effects) {
		 if (idx == effect->handle->slot) {
			 return effect;
		 }
	}
	return NULL;
}

vstplugin* track_plugins_t::getPluginById(int32_t projectGlobalId) {
	if (instrument && instrument->projectGlobalId == projectGlobalId) {
		return instrument;
	}
	for (vstplugin* effect : effects) {
		 if (effect->projectGlobalId == projectGlobalId) {
			 return effect;
		 }
	}
	return NULL;
}
vstplugin* track_plugins_t::setInstrument(vstplugin* _instrument) {
	vstplugin* oldInstr = instrument;
	if (instrument) {
		removePlugin(instrument);
	}
	instrument = _instrument;
	_instrument->handle->tr_plugins = this;
	_instrument->handle->slot = 0;
	return oldInstr;
}
void track_plugins_t::removePlugin(vstplugin* _vst) {
	if (this->selectedAutomationCtr == _vst) {
		this->selectedAutomationCtr = NULL;
	}
	if (instrument == _vst) {
		instrument = NULL;
	} else {
		if (!removeEntry(effects, _vst)) {
			return;
		}
		int slot = 1;
		for (vstplugin* effect : effects) {
			effect->handle->slot = slot++;
		}
	}
	_vst->handle->tr_plugins = NULL;
	_vst->handle->slot = -1;
	guiplugin* gui = _vst->handle->gui.get();
	if (gui) {
		guictr_plugins* plugins = MainCtrl::getPluginCtr();
		if (plugins && plugins->hasGui(gui)) {
			plugins->remove(gui);
			plugins->layout();
		}
	}
	MainCtrl::getGuiTrackCtr()->removeAllAutomationLanes(this->track, _vst);
	MainCtrl::getGuiTrackCtr()->layout();
	MainCtrl::getGuiTrackCtr()->updateVisibleTrackContents();
}

void track_plugins_t::insertEffect(int32_t idx, vstplugin* _effect) {
	std::vector<vstplugin*>::iterator it;
	if (idx == -2 || idx >= (int32_t)effects.size()) {
		it = effects.end();
	} else if (idx <= 0) {
		it = effects.begin();
	} else {
		it = effects.begin() + idx;
	}
	effects.insert(it, _effect);
	_effect->handle->tr_plugins = this;
	int slot = 1;
	for (vstplugin* effect : effects) {
		effect->handle->slot = slot++;
	}
}

struct noteevent_t {
	int32_t pitch = 0;
	tick_t tickOffsetInBlock;
	bool isNoteOn;
	bool isLoopNoteOff;
	noteevent_t(int32_t p, tick_t t, bool b, bool b2) : pitch(p), tickOffsetInBlock(t), isNoteOn(b), isLoopNoteOff(b2) {

	}
};

struct VstEvent_t {
	int32_t maxEvents;
	VstEvents* vstEvents;
	VstMidiEvent* evtArr;
	int32_t numOns = 0;
	int32_t numOffs = 0;
	VstEvent_t(size_t s) : maxEvents(s) {
		size_t hdr = sizeof(VstEvents) + sizeof(VstEvents*) * (s-2);
		size_t len = hdr + sizeof(VstMidiEvent) * (s);
		vstEvents = (VstEvents*) malloc(len);
		memset(vstEvents, 0, len);
		uint8_t* bytePtr = reinterpret_cast<uint8_t*>(vstEvents) + hdr;
		evtArr = reinterpret_cast<VstMidiEvent*>(bytePtr);
	}
	void reset() {
		numOns = numOffs = 0;
		vstEvents->numEvents = 0;
		memset(vstEvents->events, 0, sizeof(VstMidiEvent*)*maxEvents);
	}
	~VstEvent_t() {
		free(vstEvents);
	}
	void writeNoteOn(unsigned char* buf, int32_t pitch) {
		buf[0] = 0x90;
		buf[1] = CLAMP_I(pitch, 0, 0x7F);
		buf[2] = 0x7F;
		buf[3] = 0;
	}
	void writeNoteOff(unsigned char* buf, int32_t pitch) {
		buf[0] = 0x80;
		buf[1] = CLAMP_I(pitch, 0, 0x7F);
		buf[2] = 0x40;
		buf[3] = 0;
	}
	void writeVstMidiEvt(noteevent_t& nevt, double tickToSamples, int32_t blockSize) {
		int32_t idx = vstEvents->numEvents;
		assert(idx < maxEvents);
		VstMidiEvent& evt = evtArr[idx];
//		tick_t noteSamplePos = tickToSample(pos, bpm100, sampleRate, blockSize);
		evt.type = kVstMidiType;
		evt.byteSize = sizeof(VstMidiEvent);
		evt.flags = 0;//kVstMidiEventIsRealtime;
		evt.deltaFrames = floor(nevt.tickOffsetInBlock*tickToSamples);
		assert(evt.deltaFrames >= 0 && evt.deltaFrames < blockSize);
		if (nevt.isNoteOn) {
			numOns++;
			writeNoteOn((unsigned char*)evt.midiData, nevt.pitch);
		} else {
			numOffs++;
			writeNoteOff((unsigned char*)evt.midiData, nevt.pitch);
		}
		vstEvents->events[idx] = reinterpret_cast<VstEvent*>(&evt);
		vstEvents->numEvents++;
	}
	void writeMessage(char c0, char c1, char c2, char c3, int32_t delta) {
		int32_t idx = vstEvents->numEvents;
		assert(idx < maxEvents);
		VstMidiEvent& evt = evtArr[idx];
		evt.type = kVstMidiType;
		evt.byteSize = sizeof(VstMidiEvent);
		evt.flags = kVstMidiEventIsRealtime;
		evt.deltaFrames = 0;
		char* buf = evt.midiData;
		buf[0] = c0;
		buf[1] = c1;
		buf[2] = c2;
		buf[3] = c3;
		vstEvents->events[idx] = reinterpret_cast<VstEvent*>(&evt);
		vstEvents->numEvents++;
	}
	void writeInstantOff() {
		writeMessage(0xB0, 123, 0, 0, 0); // ALL NOTES OFF
		for (int32_t i = 0; i < vstEvents->numEvents; i++) {
			evtArr[i].deltaFrames = 0;
		}
	}
};

void track_plugins_t::sendNotesOff(int32_t bpm100, int32_t blockSamplePos) {
	VstEvent_t* midiEventsBuf = reallocEvts(track->audio->heldNotes.size());
	for (note_t& note : track->audio->heldNotes) {
//		midiEventsBuf->writeVstMidiEvt(note, bpm100, blockSamplePos, sampleRate, blockSize, false);
		my_printf("Send note off %d\n", note.pitch);
	}
	midiEventsBuf->writeInstantOff();
	track->audio->heldNotes.clear();
	if (instrument)
		instrument->dispatch(effProcessEvents, 0, 0, midiEventsBuf->vstEvents);
}
track_plugins_t::~track_plugins_t() {
	if (midiEventsBuf) delete midiEventsBuf;
}
VstEvent_t* track_plugins_t::reallocEvts(size_t size) {
	size = max((size_t)128, size);
	if (midiEventsBuf == NULL || midiEventsBuf->maxEvents < (int32_t)size) {
		if (midiEventsBuf) delete midiEventsBuf;
		midiEventsBuf = new VstEvent_t(size);
	}
	midiEventsBuf->reset();
	return midiEventsBuf;
}
void track_plugins_t::getAutomatableTargets(std::vector<automatable_t*>& targets) {
	targets.push_back(&mixer);
	if (instrument)
		targets.push_back(instrument);
	targets.insert(targets.end(), effects.begin(), effects.end());
}
void track_plugins_t::saveAutomationLanes(std::vector<automationlane_snapshot_t>& atls)
{
	atls.reserve(track->subtracks.size());
	for (gui_track_automationlane* atl : track->subtracks) {
		automationlane_snapshot_t ref = atl->at->toRef();
		ref.paramIdx = atl->param;
		ref.height = atl->height;
		atls.push_back(std::move(ref));
	}
}
void track_plugins_t::loadAutomationLanes(const std::vector<automationlane_snapshot_t>& atls)
{
	guictr_tracks* guiTracks = MainCtrl::getGuiTrackCtr();
	for (const automationlane_snapshot_t& ref : atls) {
		gui_track_automationlane* al = NULL;
		if (ref.type == 0) {
			vstplugin* plugin = getPluginById(ref.refId);
			if (!plugin) {
				continue;
			}
			al = guiTracks->addAutomationLane(track, plugin, ref.paramIdx, false);

		}
		if (ref.type == 1) {
			al = guiTracks->addAutomationLane(track, &mixer, ref.paramIdx, false);
		}
		if (al)
			al->height = ref.height;
	}
}
void track_plugins_t::onTick(double since) {
	meter.onTick(since);
}
void track_plugins_t::sendNotes(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos) {
	//assert(end != loopEnd); //if end equals loopEnd note off events will be on exact end
	if (instrument && instrument->bCanReceiveMidi) {
		std::vector<note_t> notes;

		 //TODO: figure out thread synchronization model
		tick_t heldBegin = start;
		tick_t heldEnd = end;
		for (const note_t& note : heldNotes) {
			heldBegin = min(heldBegin, note.start());
			heldEnd = max(heldEnd, note.end());
		}
		track->getMidi().getNotesInRange(heldBegin, heldEnd, -1, loopEnd, notes);
		if (loopStart > -1&&start<loopStart)
			start = loopStart;
		if (!notes.empty() || !heldNotes.empty()) {
			std::vector<note_t> notesBegin;
			std::vector<note_t> notesEnd;
			std::vector<noteevent_t> noteEvents;
			notesBegin.reserve(notes.size());
			notesEnd.reserve(heldNotes.size()+6);
			for (note_t& note : notes) {
				if (note.start() >= start && note.start() < end) {
					notesBegin.push_back(note);
					noteEvents.emplace_back(note.pitch, note.start()-start, true, false);
				}

				//TODO: this needs to be note.end() <= end
				//   (opposed to < for being in range)
				//   for loopends to work but may put the
				//   note event on delta sample 'blockSize'
				if (note.end() > start && note.end() <= end) {
					notesEnd.push_back(note);
					noteEvents.emplace_back(note.pitch, note.end()-start-1, false, note.end() == loopEnd);
				}
//				else if (note.end() >= start && note.end() == end) {
//					my_printf("note end is on end %d, loopEnd is %d\n", end, loopEnd);
//				}
			}

			//revalidate note ends to end notes after loop or clip modifactions
			for (const note_t& noteHeld : heldNotes) {
				bool found = false;
				for (note_t& note : notes) {
					if (note.pitch == noteHeld.pitch &&
							note.start() <= noteHeld.start() && note.end() >= start) {
						found = true;
						break;
					}
				}
				if (!found) {
					my_printf("force note end!\n", 0);
					notesEnd.push_back(noteHeld);
					noteEvents.emplace_back(noteHeld.pitch, 0, false, false);
				}
			}
//			for (note_t& note : heldNotes) {
//				if (note.end() >= start && note.end() < end) {
//					notesEnd.push_back(note);
//					noteEvents.emplace_back(note, false);
//					my_printf("%d END NOTE %d %d\n", blockSamplePos/blockSize, note.time, note.pitch);
//				}
//			}
			addAll(heldNotes, notesBegin);
			removeAll(heldNotes, notesEnd);
			std::sort(noteEvents.begin(), noteEvents.end(), [](noteevent_t& a, noteevent_t& b) {
				if (a.isNoteOn && !b.isNoteOn) {
					return false;
				}
				if (!a.isNoteOn && b.isNoteOn) {
					return true;
				}
				if (a.tickOffsetInBlock == b.tickOffsetInBlock) {
					return a.pitch < b.pitch;
				}
				return a.tickOffsetInBlock < b.tickOffsetInBlock;
			});
//			if (noteEvents.size()) {
//				my_printf("%d %d NOTES\n", start, noteEvents.size());
//			}
//			for (noteevent_t& note : noteEvents) {
//				my_printf("NOTE_%s %d %d\n", note.isNoteOn?"ON":"OFF", note.tickOffsetInBlock, note.pitch);
//			}
			size_t numEvents = noteEvents.size();
			VstEvent_t* midiEventsBuf = reallocEvts(numEvents);
			const double ticksPerBlock = toTickPrecise(blockSize/(double)sampleRate, bpm100);
			const double tickToSamples = (60*sampleRate) / (bpm100/100.0*TICKS_QUARTER);
			for (noteevent_t& evt : noteEvents) {
				assert(evt.tickOffsetInBlock >= 0 && evt.tickOffsetInBlock < ticksPerBlock);
				midiEventsBuf->writeVstMidiEvt(evt, tickToSamples, blockSize);
			}
			assert(midiEventsBuf->vstEvents->numEvents == (int32_t) numEvents);
			instrument->dispatch(effProcessEvents, 0, 0, midiEventsBuf->vstEvents);
		} else {
			static VstEvents noEvData;
			noEvData = { 0 };

			instrument->dispatch(effProcessEvents, 0, 0, &noEvData);
		}
		static VstEvents noEvData;
		for (vstplugin* eff : effects) {
			if (eff->bCanReceiveMidi&& eff->bIsEnabled) {
				noEvData = { 0 };

				instrument->dispatch(effProcessEvents, 0, 0, &noEvData);
			}
		}
	}

//	return NULL;

}
trackstate_t::~trackstate_t() {
	for (track_snapshot_t* track : tracks) {
		delete track;
	}
}
void trackstate_t::reset() {
	for (track_snapshot_t* track : tracks) {
		delete track;
	}
	tracks.clear();
}
trackstate_t trackstate_t::copy() {
	trackstate_t t;
	for (track_snapshot_t* track : tracks) {
		track_snapshot_t* trackCopy = new track_snapshot_t(*track);
		t.tracks.push_back(trackCopy);
	}
	tracks.clear();
	return t;
}

const char* trackTypeNames[4] = {
	"Master", "Return", "Midi", NULL
};
const char* TrackTypeToName(int type) {
	return trackTypeNames[type];
}
