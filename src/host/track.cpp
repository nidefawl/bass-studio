#include <algorithm>


#include "exceptions.h"
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
#include "audiocache.h"
#include "vst_plugin.h"
#include "vst_plugin_handles.h"
#include "vst_host.h"
#include "track_impl.h"

#include "mainctrl.h"
#include "plugindatabase.h"

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
	trackdata_midi_t& midi = tr->getMidi();
	std::vector<clip_t*>& clips = midi.clips;
	for (auto itClip = clips.begin(); itClip != clips.end(); itClip++) {
		clip_t* cl = *itClip;
		deleteClip(cl, cb);
	}
	clips.clear();
	if (tr->mixer) {
		delete (tr->mixer);
	}
	if (tr->content) {
		delete (tr->content);
	}
	if (tr->subtracks.size()) {
		for (gui_track_automationlane* al : tr->subtracks) {
			delete al;
		}
		tr->subtracks.clear();
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

track_t &track_t::operator =(const track_snapshot_t &obj) {
	std::vector<clip_t*>& clips = midi.clips;
	clips.clear();
	for (const clip_t& clip : obj.clips) {
		clips.push_back(new clip_t(clip));
	}
	midi.sortClips();
	tracksettings_t& dst = *static_cast<tracksettings_t*>(this);
	const tracksettings_t& src = *static_cast<const tracksettings_t*>(&obj);
	dst = src;
	scrolloffset = 0;
	return *this;
}
track_t::track_t(const track_snapshot_t &a)
  : tracksettings_t(a), localIdx(a.localIdx) {
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
	ps.enabled = plugin->bIsEnabled;
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
		if (automatedParam.src.points.empty()) {
			continue;
		}
		automation_view_t automation;
		automation.targetParam = automatedParam.paramIdx;
		automation.points = automatedParam.src.points;
		automation.active = automatedParam.src.active;
		ps.automatedParams.push_back(automation);
	}
}
track_impl_snapshot_t::track_impl_snapshot_t(const track_t &a, bool storePluginChunks) {
	track_impl_t* p = a.audio;
	if (p) {
		p->mixer.createSnapshot(trackParams);
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
  : tracksettings_t(*track), localIdx(track->localIdx), plugins(*track, storePluginChunks)
{
	std::vector<clip_t*>& otherClips = track->getMidi().clips;
	for (clip_t* clip : otherClips) {
		clips.emplace_back(*clip);
	}
	track_impl_t* p = track->audio;
	if (p) {
		p->saveAutomationLanes(automationLanes);
	}
}


void track_t::loadSnapshot(const track_snapshot_t& snapshot) {
	auto audio = this->audio;
	assert(audio);
	const auto& implSnapshot = snapshot.plugins;
	audio->mixer.loadSnapshot(implSnapshot.trackParams);
	const std::vector<plugin_snapshot_t>& trPluginList = implSnapshot.plugins;
	audio->loadPlugins(trPluginList);
	const std::vector<automationlane_snapshot_t>& atl = snapshot.automationLanes;
	this->subtracks.clear();
	bool showSubtracks = !this->hideAutomation && !this->hideTrack;
	if (!showSubtracks) {
		audio->atl = atl;
		audio->atlStored = true;
	} else {
		audio->atlStored = false;
		audio->atl.clear();
		audio->loadAutomationLanes(atl);
	}
}
void track_t::loadPluginAutomationParameters(const track_impl_snapshot_t& trackStatic) {
	assert(audio);
	const std::vector<plugin_snapshot_t>& trPluginList = trackStatic.plugins;
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
void trackdata_midi_t::getClipsInRange(tick_t start, tick_t end, std::vector<clip_t*>& _clips) {
//	my_printf("range %d to %d\n", start, end);
	for (clip_t* clip : clips) {
		if (clip->end() <= start || clip->start() > end) {
			continue;
		}
		if (clip->clipType == CLIP_AUDIO) {
			_clips.push_back(clip);
		}
	}

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

vstplugin* track_impl_t::getPluginSlot(int32_t idx) {
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

vstplugin* track_impl_t::getPluginById(int32_t projectGlobalId) {
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
vstplugin* track_impl_t::setInstrument(vstplugin* _instrument) {
	vstplugin* oldInstr = instrument;
	if (instrument) {
		removePlugin(instrument, true);
	}
	instrument = _instrument;
	_instrument->handle->tr_plugins = this;
	_instrument->handle->slot = 0;
	return oldInstr;
}
void track_impl_t::removePlugin(vstplugin* _vst, bool notifyUp) {
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
	if (notifyUp) {
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
}

void track_impl_t::insertEffect(int32_t idx, vstplugin* _effect) {
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

void track_impl_t::sendNotesOff(int32_t bpm100, int32_t blockSamplePos) {
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
track_impl_t::~track_impl_t() {
	if (midiEventsBuf) delete midiEventsBuf;
	my_printf("~track_impl_t() %016X\n", this);
}
VstEvent_t* track_impl_t::reallocEvts(size_t size) {
	size = max((size_t)128, size);
	if (midiEventsBuf == NULL || midiEventsBuf->maxEvents < (int32_t)size) {
		if (midiEventsBuf) delete midiEventsBuf;
		midiEventsBuf = new VstEvent_t(size);
	}
	midiEventsBuf->reset();
	return midiEventsBuf;
}
int32_t track_impl_t::getLatency() {
	return latency;
}
void track_impl_t::pluginsChanged() {
	samplerate_t latency = 0;
	if (instrument) {
		latency += instrument->handle->aeffect->initialDelay;
	}
	for (vstplugin* effect : effects) {
		latency += effect->handle->aeffect->initialDelay;
	}
	this->latency = latency;
}
void track_impl_t::getAutomatableTargets(std::vector<automatable_t*>& targets) {
	targets.push_back(&mixer);
	if (instrument)
		targets.push_back(instrument);
	targets.insert(targets.end(), effects.begin(), effects.end());
}
void track_impl_t::saveAutomationLanes(std::vector<automationlane_snapshot_t>& atls)
{
	atls.reserve(track->subtracks.size());
	for (gui_track_automationlane* atl : track->subtracks) {
		automationlane_snapshot_t ref = atl->at->toRef();
		ref.paramIdx = atl->param;
		ref.height = atl->height;
		atls.push_back(std::move(ref));
	}
}
void track_impl_t::showAutomationLanes() {
	bool hide = track->hideAutomation || track->hideTrack;
	if (this->atlStored == hide)
		return;
	this->atlStored = hide;
	if (hide) {
		atl.clear();
		saveAutomationLanes(atl);
		MainCtrl::getGuiTrackCtr()->removeAllAutomationLanes(track);
		Cursor& cursor = MainCtrl::get()->cursor;
		if (cursor.inSubTrackRange(track->idx, 0)) {
			fixCursorSubRange(cursor, 0);
		}
	} else {
		loadAutomationLanes(atl);
	}
}
void track_impl_t::loadPlugins(const std::vector<plugin_snapshot_t>& trPluginList)
{
//	return;
	vsthost* host = vsthost::getInstance();
	plugindatabase_t& db = MainCtrl::get()->plugindb;
	for (const plugin_snapshot_t& pluginSnapshot : trPluginList) {
		String path;
		if (db.resolve(pluginSnapshot.name, pluginSnapshot.uId, &path)) {
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
				host->insertNewPlugin(this, plugin, pluginSnapshot.slot);

				const std::vector<automation_view_t>& automatedParams = pluginSnapshot.automatedParams;
				for (const automation_view_t& automatedParam : automatedParams) {
					if (plugin->getParam(automatedParam.targetParam)) {
						automation_t* autom = plugin->getAutomation(automatedParam.targetParam);
						autom->points = automatedParam.points;
						autom->active = automatedParam.active;
					}
				}
				if (plugin == this->instrument) {
//					plugin->show();
				}
				if (pluginSnapshot.enabled) {
					plugin->resume();
				}
			}
		}
	}
}
void track_impl_t::loadAutomationLanes(const std::vector<automationlane_snapshot_t>& atls)
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
void track_impl_t::onTick(double since) {
	meter.onTick(since);
	if (instrument) {
		instrument->meter.onTick(since);
	}
	for (auto effect : effects) {
		effect->meter.onTick(since);
	}
}
void track_impl_t::fillAudio(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos, float** buffer, uint32_t blockSize) {

	int32_t blockEnd = blockSamplePos+blockSize;
	tick_t audioBegin = max(start, loopStart);
	tick_t audioEnd = loopEnd < 0 ? end : min(end, loopEnd);
	std::vector<clip_t*> clips;
	track->getMidi().getClipsInRange(audioBegin, audioEnd, clips);
	for (clip_t* clip : clips) {
		tick_t clipStartTick = clip->getOffsetStart();
		tick_t clipEndTick = clip->end();
		int32_t clipStartSample = tickToSample(clipStartTick, bpm100, sampleRate);
		int32_t clipEndSample = tickToSample(clipEndTick, bpm100, sampleRate);
		if (clipStartSample > blockEnd)
			continue;
		if (clipEndSample <= blockSamplePos)
			continue;
		int32_t clipEndSampleLen = std::min((int32_t)blockSize, clipEndSample-blockSamplePos);
		int32_t clipStartSampleLen = blockSize - std::max((int32_t)0, clipStartSample-blockSamplePos);
		int32_t srcStartOffset = std::max(0, blockSamplePos-clipStartSample);
		int32_t dstStartOffset = std::max(0, clipStartSample-blockSamplePos);

		cachedaudio_t* audio = audiocache::getInstance()->get(clip->audio.id);
		if (audio) {
			audiosample_t* sample = audio->sample.get();
			if (srcStartOffset >= sample->nSamples)
				continue;
			assert(sample->samples.size() == 2);
			for (int i = 0; i < 2; i++) {
				float *dst = buffer[i];
				auto& srcVector = sample->samples[i];
				int32_t len = std::min(clipEndSampleLen, std::min(clipStartSampleLen, (int32_t)srcVector.size()-srcStartOffset));
				assert(dstStartOffset+len <= blockSize);
				assert(srcStartOffset+len <= srcVector.size());
				assert(dstStartOffset>=0);
				assert(srcStartOffset>=0);
				assert(len>0);
				memcpy(dst+dstStartOffset, srcVector.data()+srcStartOffset, len*sizeof(float));
			}
		}
	}
}


void track_impl_t::sendNotes(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos) {
	//assert(end != loopEnd); //if end equals loopEnd note off events will be on exact end
	if (instrument && instrument->bCanReceiveMidi) {
		std::vector<note_t> notes;

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

				if (note.end() > start && note.end() <= end) {
					notesEnd.push_back(note);
					noteEvents.emplace_back(note.pitch, note.end()-start-1, false, note.end() == loopEnd);
				}
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

				eff->dispatch(effProcessEvents, 0, 0, &noEvData);
			}
		}
	}

//	return NULL;

}
track_t* vstplugin::getTrack() {
	return handle->tr_plugins->track;
}

const char* trackTypeNames[5] = {
	"Master", "Return", "Midi", "Audio", NULL
};
const char* TrackTypeToName(int type) {
	return trackTypeNames[type];
}
