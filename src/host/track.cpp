#include <algorithm>

#include "math/seq_math.h"
#include "exceptions.h"
#include "logging.h"
#include "samplerate.h"
#include "seq_util.h"
#include "seq_time.h"

#include "../gui/pluginctr.h"
#include "../gui/trackctr.h"
#include "../gui/trackcontrols.h"
#include "../gui/trackcontent.h"

#include "note.h"
#include "clip.h"
#include "midiarp.h"
#include "track.h"
#include "cursor.h"
#include "audiocache.h"
#include "plugin/base_plugin.h"
#include "plugin/vst_plugin.h"
#include "plugin/vst_plugin_handles.h"
#include "vst_host.h"
#include "track_impl.h"

#include "modules.h"
#include "project.h"
#include "projectcontroller.h"
#include "snapshot.h"
#include "mainctrl.h"
#include "history.h"
#include "plugindatabase.h"
#include "gui/drawwaveform.h"
#include "gui/subtrack.h"
#include "midi-msg.h"
#include "fileio.h"
#include "clip.h"
#include "assert_dbg.h"


const tick_t INVALID_TICK = 1 << 31;

void releaseClipResources(clip_t* cl, delete_cb *cb) {
	if (cb)
		cb->preClipDelete(cl);
//	if (waveformrender::getInstance()) {
//		waveformrender::getInstance()->assertWaveformRefIsUnbound(&cl->audio.waveformRef);
//	}
	std::vector<track_gui_entry_t*> copyEntries = cl->trackEntries;
	for (track_gui_entry_t* entry : copyEntries) {
		track_t* track = entry->track;
		if (track) {
			if (entry->clipsGuis.count(cl)) {
				auto* pGui = entry->clipsGuis[cl];
				dbgassert(pGui);
				entry->content->remove(pGui);
				DELETE_PTR(pGui);
			} else {
				dbgassert(0);
			}
		}
	}
}
void releaseTrackResources(track_t* tr, delete_cb *cb) {
	dbgassert(tr && tr->audio);
	if (cb)
		cb->preTrackDelete(tr);
	vsthost* host = vsthost::getInstance();
	host->unloadTrack(tr);
	tr->getMidi().deleteClips(cb);
//	if (tr->mixer) {
//		delete (tr->mixer);
//	}
//	if (tr->content) {
//		delete (tr->content);
//	}
	dbgassert(tr->audio->guiInstances.empty());
	host->releaseAudio(tr);
	dbgassert(tr && !tr->audio);
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
	return minmax.first ? minmax.first->time : 0;
}
tick_t trackdata_midi_t::end() {
	auto minmax = getMinMax();
	return minmax.second ? minmax.second->end() : 0;
}

track_t &track_t::operator =(const track_snapshot_t &obj) {
	dbgassert(midi.getConstClips().empty());
	for (const clip_t& clip : obj.clips) {
		midi.addClip(new clip_t(clip));
	}
	midi.sortClips();
	tracksettings_t& dst = *static_cast<tracksettings_t*>(this);
	const tracksettings_t& src = *static_cast<const tracksettings_t*>(&obj);
	dst = src;
	scrolloffset = 0;
	return *this;
}
track_t::track_t(const track_snapshot_t &a)
  : tracksettings_t(a), localIdxFlat(a.localIdx) {
	dbgassert(midi.getConstClips().empty());
	for (const clip_t& clip : a.clips) {
		midi.addClip(new clip_t(clip));
	}
}
track_impl_snapshot_t::track_impl_snapshot_t(track_impl_t* p, bool storePluginChunks) {
	if (p) {
		p->arp->createSnapshot(trackArp);
		p->mixer.createSnapshot(trackParams);
		p->createIOSnapshot(trackIO);
		std::vector<effectbase*> effects = p->effects;
		pluginSnapshots.reserve(p->effects.size());
		for (effectbase* effect : p->effects) {
			plugin_snapshot_t ps;
			effect->makeSnapshot(ps, storePluginChunks);
			pluginSnapshots.push_back(std::move(ps));
		}
	}
}

void saveSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t& entry, track_layout_snapshot_t& snapshot);

track_snapshot_t::track_snapshot_t(const track_t* track, bool storePluginChunks)
  : tracksettings_t(*track), stageId(track->audio ? static_cast<int32_t>(track->audio->stageId) : 0), localIdx(track->localIdxFlat), plugins(track->audio, storePluginChunks)
{
	auto& otherClips = track->getConstMidi().getConstClips();
	for (auto clip : otherClips) {
		clips.emplace_back(*clip);
	}
	track_impl_t* p = track->audio;
	if (p) {

		for (int i = 0; i < 32; i++) {
			guictr_tracks* ctr = DawInstance::get()->getTrackContainer(i);
			if (ctr) {
				track_gui_entry_t out;
				if (ctr->guiMgr.getTrackEntry(track, out)) {
					track_layout_snapshot_t snapshot;
					saveSubtrackLayout(ctr, out, snapshot);
					layouts[i] = snapshot;
				}
			}
		}
	}
}


void track_t::loadSnapshot(const track_snapshot_t& snapshot) {
	auto audio = this->audio;
	dbgassert(audio);
	const auto& implSnapshot = snapshot.plugins;
	//TODO: test if stageId is in use. Caller is responsible for generating new stageId
	if (snapshot.stageId > 0) {
		audio->stageId = static_cast<audiostageid_i32>(snapshot.stageId);
	} else {
		// corrupt snapshot, keep stage id
	}

	audio->mixer.loadSnapshot(implSnapshot.trackParams);
	audio->arp->loadSnapshot(implSnapshot.trackArp);
	const std::vector<plugin_snapshot_t>& trPluginList = implSnapshot.pluginSnapshots;
	audio->loadPlugins(trPluginList);
	audio->loadIOConfiguration(implSnapshot.trackIO);
}
//void track_t::loadPluginAutomationParameters(const track_impl_snapshot_t& trackStatic) {
//	dbgassert(audio);
//	dbgassert(0&&"NOT IMPLEMENTED");
////	const std::vector<plugin_snapshot_t>& trPluginList = trackStatic.pluginSnapshots;
////	for (const plugin_snapshot_t& pluginSnapshot : trPluginList) {
////		effectbase* effect = audio->getPluginById(pluginSnapshot.projectGlobalId);
////		if (effect) {
////			const std::vector<automation_view_t>& automatedParams = pluginSnapshot.automatedParams;
////			for (const automation_view_t& automatedParam : automatedParams) {
////				if (effect->getParam(automatedParam.targetParam)) {
////					automation_t* autom = effect->getAutomation(automatedParam.targetParam);
////					autom->points = automatedParam.points;
////				}
////			}
////		}
////	}
//
//}
void track_t::releaseTrackContent() {
}
void trackdata_midi_t::deleteClips(delete_cb *cb) {
	for (auto clip : clips) {
		releaseClipResources(clip, cb);
	}
	for (auto clip : clips) {
		delete clip;
	}
	clips.clear();
}
void trackdata_midi_t::deleteEmptyClips(delete_cb *cb) {
	std::vector<clip_t*>::iterator it = clips.begin();
	while (it != clips.end()) {
		clip_t* c = *it;
		if (c->getLen() <= 0) {
			it = removeClip(c);
			releaseClipResources(c, cb);
			delete c;
		} else {
			it++;
		}
	}
	sortClips();
}
void trackdata_midi_t::getClipsInRange(tick_t start, tick_t end, std::vector<clip_t*>& _clips) {
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
	for (clip_t* clip : clips) {
		if (clip->end() <= start || clip->start() > end) {
			continue;
		}
		clip->getInTimeRange(start, end, cutStart, cutEnd, notes);
	}

}

audio_stage_ref_t audio_stage_t::toRef() {
	return {this->stageId};
}
effectbase* audio_stage_t::getPluginById(int32_t projectGlobalId) {
	for (effectbase* effect : effects) {
		 if (effect->projectGlobalId == projectGlobalId) {
			 return effect;
		 }
	}
	for (audio_stage_t* t : children) {
		auto* effect = t->getPluginById(projectGlobalId);
		if (effect) return effect;
	}
	return nullptr;
}
void track_impl_t::removePlugin(effectbase* _effect, bool notifyUp) {
	for (auto gui : track->audio->guiInstances) {
		gui->state.selectedAutomationCtr = nullptr;
	}
	audio_stage_t::removePlugin(_effect, notifyUp);
}
void audio_stage_t::removePlugin(effectbase* _effect, bool notifyUp) {
	removeEntry(deferredEffects, _effect);
	if (!removeEntry(effects, _effect)) {
		return;
	}
	int slot = 0;
	for (effectbase* effect : effects) {
		effect->setSlot(slot++);
	}
	_effect->breakTrackLink();
}

bool audio_stage_t::replaceEffect(int32_t idx, effectbase* _effect, effectbase** _prevEffect) {
	dbgassert(idx >= 0 && idx < (int32_t)effects.size());
	if (idx >= 0 && idx < (int32_t)effects.size()) {
		auto cur = effects[idx];
		cur->breakTrackLink();
		*_prevEffect = cur;
		effects[idx] = _effect;
		_effect->setTrackLink(this);
		int slot = 0;
		for (effectbase* effect : effects) {
			effect->setSlot(slot++);
		}
		return true;
	}
	return false;
}
void audio_stage_t::insertEffect(int32_t idx, effectbase* _effect) {
	std::vector<effectbase*>::iterator it;
	if (idx == -2 || idx >= (int32_t)effects.size()) {
		it = effects.end();
	} else if (idx <= 0) {
		it = effects.begin();
	} else {
		it = effects.begin() + idx;
	}
	effects.insert(it, _effect);
	_effect->setTrackLink(this);
	int slot = 0;
	for (effectbase* effect : effects) {
		effect->setSlot(slot++);
	}
}


struct VstEvent_t {
	int32_t maxEvents;
	VstEvents* vstEvents;
	VstMidiEvent* evtArr;
	int32_t numOns = 0;
	int32_t numOffs = 0;
	VstEvent_t(size_t s) : maxEvents(s) {
		/**
		 * Allocates following struct equivalent to:
			struct VstEvents
			{
				VstInt32 numEvents;		///< number of Events in array
				VstIntPtr reserved;		///< zero (Reserved for future use)
				VstEvent* events[maxEvents];	///< event pointer array, variable size
				VstMidiEvent midiEvents[maxEvents];
			};
		 */

		size_t hdr = sizeof(VstEvents) + sizeof(VstEvent*) * (s-2);
		size_t len = sizeof(VstMidiEvent) * (s);
		vstEvents = static_cast<VstEvents*>(malloc(hdr));
		evtArr = static_cast<VstMidiEvent*>(malloc(len));
		memset(vstEvents, 0, hdr);
		memset(evtArr, 0, len);
	}
	void reset() {
		numOns = numOffs = 0;
//		vstEvents->numEvents = 0;
//		memset(vstEvents->events, 0, sizeof(VstEvent)*maxEvents);
		memset(vstEvents, 0, sizeof(VstEvents) + sizeof(VstEvent*) * (maxEvents-2));
		memset(evtArr, 0, sizeof(VstMidiEvent) * (maxEvents));
	}
	~VstEvent_t() {
		free(vstEvents);
		free(evtArr);
	}
	void writeNoteOn(unsigned char* buf, int32_t pitch, int32_t velocity) {
		buf[0] = 0x90;
		buf[1] = CLAMP_I(pitch, 0, 0x7F);
		buf[2] = CLAMP_I(velocity, 0, 0x7F);
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
		dbgassert(idx < maxEvents);
		VstMidiEvent& evt = evtArr[idx];
		evt.type = kVstMidiType;
		evt.byteSize = 24;//sizeof(VstMidiEvent);
		evt.flags = 0;//kVstMidiEventIsRealtime;
		evt.deltaFrames = floor(nevt.tickOffsetInBlock*tickToSamples);
		dbgassert(evt.deltaFrames >= 0 && evt.deltaFrames < blockSize);
		if (nevt.isNoteOn) {
			numOns++;
			writeNoteOn((unsigned char*)evt.midiData, nevt.pitch, nevt.velocity);
		} else {
			numOffs++;
			writeNoteOff((unsigned char*)evt.midiData, nevt.pitch);
		}
		vstEvents->events[idx] = reinterpret_cast<VstEvent*>(&evt);
		vstEvents->numEvents++;
	}
	void writeMessage(unsigned char c0, unsigned char c1, unsigned char c2, unsigned char c3, int32_t delta) {
		int32_t idx = vstEvents->numEvents;
		dbgassert(idx < maxEvents);
		VstMidiEvent& evt = evtArr[idx];
		evt.type = kVstMidiType;
		evt.byteSize = 24;//sizeof(VstMidiEvent);
		evt.flags = 0;//kVstMidiEventIsRealtime
		evt.deltaFrames = 0;
		unsigned char* buf = (unsigned char*)evt.midiData;
		buf[0] = c0;
		buf[1] = c1;
		buf[2] = c2;
		buf[3] = c3;
		vstEvents->events[idx] = reinterpret_cast<VstEvent*>(&evt);
		vstEvents->numEvents++;
	}
	void writeInstantOff() {
		writeMessage(0xB0, 123, 0, 0, 0); // ALL NOTES OFF
//		for (int32_t i = 0; i < vstEvents->numEvents; i++) {
//			evtArr[i].deltaFrames = 0;
//		}
	}
};

track_impl_t::~track_impl_t() {
	if (midiEventsBuf) {
		delete midiEventsBuf;
	}
	delete arp;
}
VstEvent_t* track_impl_t::reallocEvts(size_t size) {
	size = math::max((size_t)128, size);
	if (midiEventsBuf == NULL || midiEventsBuf->maxEvents < (int32_t)size) {
		if (midiEventsBuf) delete midiEventsBuf;
		midiEventsBuf = new VstEvent_t(size);
	}
	midiEventsBuf->reset();
	return midiEventsBuf;
}
samplerate_t audio_stage_t::getLatency() {
	return latency;
}
void audio_stage_t::pluginsChanged() {
	samplerate_t latency = 0;
	for (effectbase* effect : effects) {
		latency += effect->getDelay();
	}
	this->latency = latency;
}
void audio_stage_t::getStageTargets(std::vector<automatable_t*>& targets) {
	if (std::find(targets.begin(), targets.end(), &mixer) == targets.end()) {
		targets.push_back(&mixer);
	}
	for (effectbase* child : effects) {
		targets.push_back(child);
		std::vector<audio_stage_t*> childStages;
		child->getChildAudioStages(childStages);
		for (audio_stage_t* childStage : childStages) {
			childStage->getStageTargets(targets);
		}
	}
}
void track_impl_t::getAutomatableTrackTargets(std::vector<automatable_t*>& targets) {
	targets.push_back(&mixer);
	targets.push_back(arp);
	getStageTargets(targets);
}

void project_t::copyTo(project_snapshot_t& project) {
	trackList.copyTo(project);
}
void project_t::copyFrom(project_snapshot_t& project) {
	trackList.copyFrom(project);
}

effectbase* loadEffectModule(const plugin_snapshot_t& pluginSnapshot, bool forceLoad) {
	vsthost* host = vsthost::getInstance();
	String path;
	effectbase* effect = nullptr;
	vstplugin* loadedPlugin = nullptr;
	if (pluginSnapshot.pluginType == PLUGIN_TYPE_VST) {
		my_printf("Next loading plugin %s, uId %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId);
		plugindatabase_t* db = plugindatabase_t::getInstance();
		if (db->resolve(pluginSnapshot.name, pluginSnapshot.uId, &path, forceLoad ? 1 : 0)) {
			my_printf("Plugin is registered... loading %s, uId %d, forceLoad %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId, forceLoad);
			vstpluginloadres res = host->loadPlugin(path, pluginSnapshot.uId, pluginSnapshot.projectGlobalId);
			if (res.result==0&&res.plugin) {
				loadedPlugin = res.plugin;
				effect = res.plugin;
			} else {
				my_printf("Failed loading: Error loading plugin %s, uId %d. Res: %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId, res.result);

			}
		} else {
			my_printf("Failed loading: Unknown plugin %s, uId %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId);

		}
	} else {
		effect = host->makeModuleInstance(pluginSnapshot.pluginType, pluginSnapshot.uId, pluginSnapshot.projectGlobalId);
		if (effect && effect->getModuleType() == PLUGIN_TYPE_INTERNAL_EFFECT) {
			loadedPlugin = dynamic_cast<vstplugin*>(effect);
		}
	}
	if (loadedPlugin && (loadedPlugin->getFlagsVST() & effFlagsProgramChunks) != 0) {
		if (pluginSnapshot.dataChunk.size() > 0) {
			my_printf("Plugin %s: Load data1[%d]\n", StringAsCStr(loadedPlugin->sName), pluginSnapshot.dataChunk.size());
			loadedPlugin->dispatch(effSetChunk, 0, pluginSnapshot.dataChunk.size(), (void*)pluginSnapshot.dataChunk.data());
		}
		if (loadPluginPresetWithSnapshot && pluginSnapshot.dataChunk2.size() > 0) {
			my_printf("Plugin %s: Load data2[%d]\n", StringAsCStr(loadedPlugin->sName), pluginSnapshot.dataChunk2.size());
			loadedPlugin->dispatch(effSetChunk, 1, pluginSnapshot.dataChunk2.size(), (void*)pluginSnapshot.dataChunk2.data());
		}
	}
	return effect;
}
void loadEffectParamsFromSnapshot(const plugin_snapshot_t& pluginSnapshot, effectbase* effect) {
	const std::vector<param_snapshot_t>& pluginSnapshotParams = pluginSnapshot.params;
	for (const param_snapshot_t& param : pluginSnapshotParams) {
		automatable_param_t* atParam = effect->getParam(param.idx);
		dbgassert(atParam);
		if (atParam) {
			dbgassert(param.val >= 0.0f && param.val <= 1.0f);
			effect->setParamValue(atParam->idx, param.val, FLG_PAR_UPDATE_INIT);
		}
	}
//	const std::vector<param_snapshot_t>& pluginHostSideParams = pluginSnapshot.hostParams;
//	for (const param_snapshot_t& param : pluginHostSideParams) {
//		automatable_param_t* atParam = effect->getParam(param.idx);
//		if (atParam) {
//			effect->setParamValue(atParam->idx, param.val, FLG_PAR_UPDATE_INIT);
//		}
//	}

}
void track_impl_t::createIOSnapshot(track_io_configuration_snapshot_t& snapshot) {
	for (int i = 0; i < 2; i++) {
		DAW::channel_ref_t channel = i == 0 ? inputChannel : outputChannel;
		io_configuration_snapshot_t& cfg = i == 0 ? snapshot.input : snapshot.output;
		cfg.inputType = static_cast<int32_t>(channel.type);
		cfg.channelOffset = channel.inputChannelOffset;
		cfg.stageId = static_cast<int32_t>(channel.stage.stageRef.stageId);
		cfg.stageEndPointType = channel.stage.isInput ? 0 : 1;
		cfg.externalInputId = channel.externalInputIdx;
		cfg.externalInputType = static_cast<int32_t>(channel.externalInputType);
	}
}
void track_impl_t::loadIOConfiguration(const track_io_configuration_snapshot_t& snapshot)
{
	for (int i = 0; i < 2; i++) {
		DAW::channel_ref_t& channel = i == 0 ? inputChannel : outputChannel;
		io_configuration_snapshot_t cfg = i == 0 ? snapshot.input : snapshot.output;
		channel.type = static_cast<DAW::channel_input_type>(cfg.inputType);
		channel.inputChannelOffset = cfg.channelOffset;
		channel.stage.stageRef.stageId = static_cast<audiostageid_i32>(cfg.stageId);
		channel.stage.isInput = cfg.stageEndPointType != 0;
		channel.externalInputIdx = cfg.externalInputId;
		channel.externalInputType = static_cast<AudioIO::tracktype>(cfg.externalInputType);
	}

}
void audio_stage_t::loadPlugins(const std::vector<plugin_snapshot_t>& trPluginList)
{
	for (const plugin_snapshot_t& pluginSnapshot : trPluginList) {
		auto effect = loadPluginDeferred(pluginSnapshot);
		if (effect) {
			vsthost* host = vsthost::getInstance();

			this->deferredEffects.push_back(effect);
			host->addDeferredEffect(effect);
			effect->load(host);
			host->insertNewPlugin(this, effect, pluginSnapshot.slot);
			dbgassert(effect->trackImpl == this);
			dbgassert(effects.size());
		}
	}


}
void pluginUpdateParamBypass(effectbase* effect, int state);
void vsthost::activateDeferred(effectbase* const eff, effectbase** out_effectLoaded, bool forceLoad) {
	dbgassert(eff->trackImpl);
	dbgassert(eff->trackImpl->effects.size());
	dbgassert(eff->getSlot() >= 0);
	auto defEffect = dynamic_cast<effect_deferred*>(eff);
	plugin_snapshot_t pluginSnapshot = defEffect->getSnapshot();
	log_printf("activating deferred plugin loadEffectModule %s\n", StringAsCStr(pluginSnapshot.name));
	effectbase* effect = loadEffectModule(pluginSnapshot, forceLoad);
	if (out_effectLoaded) {
		*out_effectLoaded = effect;
	}
	if (!effect) {
		log_printf("Failed loading %s\n", StringAsCStr(pluginSnapshot.name));
//		dbgassert(0);
		return;
	}
	log_printf("activating deferred plugin loadEffectParamsFromSnapshot %s\n", StringAsCStr(pluginSnapshot.name));
	loadEffectParamsFromSnapshot(pluginSnapshot, effect);
	effectbase* prevPlugin = nullptr;
	always_assert(removeEntry(eff->trackImpl->deferredEffects, eff));
	replacePlugin(eff->trackImpl, effect, defEffect->getSlot(), &prevPlugin);
	always_assert(removeEntry(this->pluginsDeferred, eff));
	effect->loadSnapshot(pluginSnapshot);
	effect->sName = pluginSnapshot.name;
	pluginUpdateParamBypass(effect, pluginSnapshot.enabled);
	loadAutomation(pluginSnapshot.automatedParams, effect);
	if (pluginSnapshot.enabled) {
		effect->resume();
	}
	log_printf("done activating deferred plugin %s\n", StringAsCStr(pluginSnapshot.name));

}
int loadSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t& entry, const track_layout_snapshot_t& snapshot)
{
	const std::vector<automationlane_snapshot_t>& atls = snapshot.automationLanes;
	track_t* const track = entry.track;
	int n = atls.size();
	n = 0;
	for (const automationlane_snapshot_t& ref : atls) {
		gui_track_subtrack* al = NULL;
		if (ref.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
			if (ref.type == AUTOMATABLE_EFFECT) {
				effectbase* plugin = track->getStage()->getPluginById(ref.refId);
				if (!plugin || plugin->getModuleType() == PLUGIN_TYPE_DEFERRED) {
					log_printf("skipping deferred ref.type %d, ref.refId %d, ref.paramIdx %d\n", ref.type, ref.refId, ref.paramIdx);
					n++;
					continue;
				}
				al = new gui_track_automationlane(entry, guiTracks->grid, plugin, ref.paramIdx);
			}
			if (ref.type == AUTOMATABLE_MIXER) {
				al = new gui_track_automationlane(entry, guiTracks->grid, &track->getStage()->mixer, ref.paramIdx);
			}
			if (ref.type == AUTOMATABLE_ARP) {
				al = new gui_track_automationlane(entry, guiTracks->grid, track->getStage()->arp, ref.paramIdx);
			}
		} else if (ref.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_WAVE) {
			al = makeGuiSubtrack(entry, MainCtrl::get(), ref.subtrackType);
		}
		if (al) {
			al->height = ref.height;
			guiTracks->addSubTrack(entry, al, false);
		}
	}

	return n;
}

void saveSubtrackLayout(guictr_tracks* guiTracks, track_gui_entry_t& entry, track_layout_snapshot_t& snapshot)
{
	snapshot.automationLanes.reserve(entry.subtracks.size());
	for (gui_track_subtrack* atl : entry.subtracks) {
		automationlane_snapshot_t subtrackSnapshot;
		if (atl->subtrackType() == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
			dbgassert(atl->at);
			subtrackSnapshot = atl->at->toRef();
		} else {
		}
		subtrackSnapshot.paramIdx = atl->param;
		subtrackSnapshot.height = atl->height;
		subtrackSnapshot.subtrackType = atl->subtrackType();
		log_printf("save ref.type %d, ref.refId %d, ref.paramIdx %d\n", subtrackSnapshot.type, subtrackSnapshot.refId, subtrackSnapshot.paramIdx);
		snapshot.automationLanes.push_back(std::move(subtrackSnapshot));
	}

}

void updateStoreLoadSubtracks(guictr_tracks* guiTracks, track_gui_entry_t& entry) {
	bool hide = entry.layout.hideSubtracks || entry.layout.hideTrack;
	if (entry.state.wasInHide == hide)
		return;
	entry.state.wasInHide = hide;
	if (hide) {
		entry.state.layoutSaved = track_layout_snapshot_t();
		saveSubtrackLayout(guiTracks, entry, entry.state.layoutSaved);
		guiTracks->removeAllSubtracks(entry);
		DAW::Cursor& cursor = entry.parentCtrl->getCursor();
		if (cursor.inSubTrackAny(entry.track->idx)) {
			fixCursorSubRange(cursor, 0);
		}
	} else {
		loadSubtrackLayout(guiTracks, entry, entry.state.layoutSaved);
	}
}
void audio_stage_t::onTick(double since) {
	meter.onTick(since);
	meterInput.onTick(since);
	for (auto effect : effects) {
		effect->onTick(since);
	}
}
void audio_stage_t::addAudioStage(audio_stage_t* _child) {
	auto it = std::find(children.begin(), children.end(), _child);
	if (it != children.end()) {
		throw applogicexception("attempt to add audio_stage_t twice");
	}
	_child->parent = this;
	this->children.push_back(_child);
}
void audio_stage_t::removeAudioStage(audio_stage_t* _child) {
	auto it = std::find(children.begin(), children.end(), _child);
	if (it == children.end()) {
		if (_child->parent == nullptr)
			return;
		throw applogicexception("attempt to remove non-present audio_stage_t");
	}
	_child->parent = nullptr;
	this->children.erase(it);
}
track_t* audio_stage_t::getTrack() {
	audio_stage_t* stage = this;
	while (stage->parent) {
		stage = stage->parent;
	}
	if (stage->type == 0) {
		return static_cast<track_impl_t*>(stage)->track;
	}
//	dbgassert(0); // to be expected when deleting effectgroups
	return nullptr;
}
void track_impl_t::addAudio(const AudioBlock& src, float fGain) {
	const auto numChannels = math::min(src.channels, input.channels);
	for (auto channel = 0; channel < numChannels; channel++) {
		float* pChSrc = src.buf[channel];
		float* pChDst = input.buf[channel];
		dbgassert(src.samples == input.samples);
		const int32_t nSamples = math::min(src.samples, input.samples);
		for (int sample = 0; sample < nSamples; sample++) {
			*pChDst++ += (*pChSrc++)*fGain;
		}
	}

}
void track_impl_t::fillAudio(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos, float** buffer, int32_t blockSize) {

	int32_t blockEnd = blockSamplePos+blockSize;
	tick_t audioBegin = math::max(start, loopStart);
	tick_t audioEnd = loopEnd < 0 ? end : math::min(end, loopEnd);
	std::vector<clip_t*> clips;
	track->getMidi().getClipsInRange(audioBegin, audioEnd, clips);
	for (clip_t* clip : clips) {
		tick_t clipStartTick = clip->getOffsetStart();
		tick_t clipEndTick = clip->end();
		int32_t clipStartSample = tickToSample(clipStartTick, bpm100, sampleFormat.sampleRate);
		int32_t clipEndSample = tickToSample(clipEndTick, bpm100, sampleFormat.sampleRate);
		if (clipStartSample > blockEnd)
			continue;
		if (clipEndSample <= blockSamplePos)
			continue;
		int32_t clipEndSampleLen = math::min((int32_t)blockSize, clipEndSample-blockSamplePos);
		int32_t clipStartSampleLen = blockSize - math::max((int32_t)0, clipStartSample-blockSamplePos);
		int32_t srcStartOffset = blockSamplePos-clipStartSample + clip->offsetSamples;
		int32_t dstStartOffset = math::max(0, clipStartSample-blockSamplePos);
		if (srcStartOffset+blockSize <= 0)
			continue;

		audiofile_t* audio = audiocache::getInstance()->get(clip->audio.id);
		if (audio) {
			audiosample_t* sample = audio->sample.get();
			if (srcStartOffset >= (int32_t)sample->nSamples)
				continue;
			dbgassert(sample->samples.size() > 0);
			for (int i = 0; i < this->input.channels; i++) {
				float *dst = buffer[i];
				auto& srcVector = i >= (int)sample->samples.size() ? sample->samples[sample->samples.size()-1] : sample->samples[i];
				int32_t len = math::min((int32_t)blockSize-math::max(0, -srcStartOffset),
								math::min(clipEndSampleLen, math::min(clipStartSampleLen, (int32_t)srcVector.size()-srcStartOffset)));
				dbgassert(len>=0);
				if (len <= 0) { //TODO: could figure this out outside the loop
					continue;
				}
				dbgassert(dstStartOffset+len <= (int32_t)blockSize);
				dbgassert(srcStartOffset+len <= (int32_t)srcVector.size());
				dbgassert(dstStartOffset>=0);
				memcpy(dst+dstStartOffset, srcVector.data()+math::max(0, srcStartOffset), len*sizeof(float));
			}
		}
	}
}
void sortNoteEvents(std::vector<noteevent_t>& noteEvents) {
	std::sort(noteEvents.begin(), noteEvents.end(), [](const noteevent_t& a, const noteevent_t& b) {
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
}
track_impl_t::track_impl_t(audiostageid_i32 _id, track_t* _track, const samplerate_t _sampleRate, const uint16_t _blockSize, int32_t nChannels)
   : audio_stage_t(_id, /*_track, */_sampleRate, _blockSize, nChannels, 0)
  , track(_track), inputChannel(DAW::ChannelDefaultNone()), outputChannel(DAW::ChannelDefaultNone())
{
	arp = new midiarp(this);
}
//TODO: make threadsafe getters
std::vector<note_t>& track_impl_t::getArpInputNotes() {
	return this->arp->heldInputAnimationNotes;
}
//TODO: make threadsafe getters
std::vector<note_t>& track_impl_t::getArpHeldNotes() {
	return this->arp->heldOutputAnimationNotes;
}
//TODO: make threadsafe getters
std::vector<marker_t>& track_impl_t::getArpMarkers() {
	return this->arp->markers;
}
void track_impl_t::sendNotesOff(int32_t bpm100) {
	std::vector<note_t> heldNotes = track->audio->heldNotes;
	track->audio->heldNotes.clear();
	if (arp)
		arp->allNotesOff();

	std::vector<noteevent_t> noteEvents;
	noteEvents.reserve(heldNotes.size());
	for (note_t& noteHeld : heldNotes) {
		noteEvents.emplace_back(noteHeld.pitch, 0, 0, false, false);
	}
	sortNoteEvents(noteEvents);
	const double ticksPerBlock = toTickPrecise(sampleFormat.blockSize/(double)sampleFormat.sampleRate, bpm100);
	const double tickToSamples = (60.0*sampleFormat.sampleRate) / (bpm100/100.0*TICKS_QUARTER);
	VstEvent_t* midiEventsBuf = reallocEvts(noteEvents.size());
	for (noteevent_t& evt : noteEvents) {
		dbgassert(evt.tickOffsetInBlock >= 0 && evt.tickOffsetInBlock < ticksPerBlock);
		midiEventsBuf->writeVstMidiEvt(evt, tickToSamples, sampleFormat.blockSize);
	}
	dbgassert(midiEventsBuf->vstEvents->numEvents == (int32_t) noteEvents.size());
	midiEventsBuf->writeInstantOff();
	for (effectbase* effect : effects) {
		vstplugin* vst = dynamic_cast<vstplugin*>(effect);
		if (vst && vst->bCanReceiveMidi) {
//					VstEvent_t midiEventsBufTemp = *midiEventsBuf; //TODO: make a copy, plugin may manipulate data
//			log_printf("send %d midi events to %s\n", midiEventsBuf->vstEvents->numEvents, StringAsCStr(vst->getName()));
			vst->dispatch(effProcessEvents, 0, 0, midiEventsBuf->vstEvents);
		}
	}
}
void track_impl_t::sendNotes(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos,
		clip_notes_t& midiRealtimeInput, int32_t flags) {
	//dbgassert(end != loopEnd); //if end equals loopEnd note off events will be on exact end
	if (std::any_of(effects.begin(), effects.end(), [](const effectbase* ref){
			return ref->bCanReceiveMidi;
	})) {
		std::vector<note_t> notes;
		hires_timer_t tmr;

		tick_t heldBegin = start;
		tick_t heldEnd = end;
		for (const note_t& note : heldNotes) {
			heldBegin = math::min(heldBegin, note.start());
			heldEnd = math::max(heldEnd, note.end());
		}
		static int64_t time1=0;
		static int64_t time2=0;
		static int64_t time3=0;
		static int64_t time4=0;
		static int64_t time5=0;
		static int64_t time6=0;
		static int64_t time7=0;
		tmr.reset();
		if (flags & MidiFlags::PROCESS_CLIPS) {
			track->getMidi().getNotesInRange(heldBegin, heldEnd, -1, loopEnd, notes);
		}
		time1 = (time1 * 19 + tmr.getTime()) / 20;
		track->getStage()->procStats.timeGetNotesInRange = time1;

		tmr.reset();
		if (flags & MidiFlags::PROCESS_REALTIME) {
			getClipNotesInTimeRange(heldBegin, heldEnd, -1, loopEnd, midiRealtimeInput, notes);
		}
		time2 = (time2 * 19 + tmr.getTime()) / 20;


		if (loopStart > -1&&start<loopStart)
			start = loopStart;
		if (!notes.empty() || !heldNotes.empty() || arp != nullptr) {
			std::vector<note_t> notesBegin;
			std::vector<note_t> notesEnd;
			std::vector<noteevent_t> noteEvents;
			notesBegin.reserve(notes.size());
			notesEnd.reserve(heldNotes.size()+6);
			tmr.reset();
			for (note_t& note : notes) {
				if (note.start() >= start && note.start() < end) {
					notesBegin.push_back(note);
					noteEvents.emplace_back(note.pitch, note.velocity, note.start()-start, true, false);
				}

				if (note.end() > start && note.end() <= end) {
					notesEnd.push_back(note);
					noteEvents.emplace_back(note.pitch, note.velocity, note.end()-start-1, false, note.end() == loopEnd);
				}
			}
			time3 = (time3 * 19 + tmr.getTime()) / 20;

			tmr.reset();
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
					noteEvents.emplace_back(noteHeld.pitch, 0, 0, false, false);
				}
			}
			time4 = (time4 * 19 + tmr.getTime()) / 20;

			tmr.reset();
			addAll(heldNotes, notesBegin);
			removeAll(heldNotes, notesEnd);
			sortNoteEvents(noteEvents);
			time5 = (time5 * 19 + tmr.getTime()) / 20;

			tmr.reset();
			std::vector<noteevent_t> noteEventsProcessed;
			if (flags & MidiFlags::PROCESS_ARP) {
				arp->process(noteEvents, start, end, loopStart, loopEnd, noteEventsProcessed);
			} else {
				noteEventsProcessed = std::move(noteEvents);
			}
			time6 = (time6 * 19 + tmr.getTime()) / 20;
			tmr.reset();
			size_t numEvents = noteEventsProcessed.size();
			if (numEvents > 0)
			{
				VstEvent_t* midiEventsBuf = reallocEvts(numEvents);
				const double ticksPerBlock = toTickPrecise(sampleFormat.blockSize/(double)sampleFormat.sampleRate, bpm100);
				const double tickToSamples = (60.0*sampleFormat.sampleRate) / (bpm100/100.0*TICKS_QUARTER);
				for (noteevent_t& evt : noteEventsProcessed) {
					dbgassert(evt.tickOffsetInBlock >= 0 && evt.tickOffsetInBlock < ticksPerBlock);
					midiEventsBuf->writeVstMidiEvt(evt, tickToSamples, sampleFormat.blockSize);
				}
				dbgassert(midiEventsBuf->vstEvents->numEvents == (int32_t) numEvents);
				for (effectbase* effect : effects) {
					vstplugin* vst = dynamic_cast<vstplugin*>(effect);
					if (vst && vst->isSynth) {
	//					VstEvent_t midiEventsBufTemp = *midiEventsBuf; //TODO: make a copy, plugin may manipulate data
						vst->midiEventsDispatched += midiEventsBuf->vstEvents->numEvents;
						vst->dispatch(effProcessEvents, 0, 0, midiEventsBuf->vstEvents);
					}
				}
			}
			time7 = (time7 * 19 + tmr.getTime()) / 20;
		} else {
//			for (effectbase* effect : effects) {
//				vstplugin* vst = dynamic_cast<vstplugin*>(effect);
//				if (vst && vst->bCanReceiveMidi) {
//					VstEvents noEvData;
//					noEvData = {  };
//					vst->dispatch(effProcessEvents, 0, 0, &noEvData);
//				}
//			}
		}
	}
}

void track_params_t::createSnapshot(track_params_snapshot_t& snapshot) {
	snapshot.params.reserve(getNumParameters());
	visitParams([&snapshot](auto& mapEntry) {
		auto& param = mapEntry.second;
		snapshot.params.push_back(param_snapshot_t{param.idx, param.value});
	});
	storeAutomation(snapshot.automatedParams, this);
}
void track_params_t::loadSnapshot(const track_params_snapshot_t& snapshot) {
	for (const auto& param : snapshot.params) {
		dbgassert(getParam(param.idx));
		setParamValue(param.idx, param.val, FLG_PAR_UPDATE_INIT);
	}
	loadAutomation(snapshot.automatedParams, this);
}
void track_params_t::postSetParameter(int32_t idx, float preVal, float val, int flags) {
	if (flags != FLG_PAR_UPDATE_USER) {
		return;
	}
	dbgassert(this->audiostage->getTrack());
	track_t* track = this->audiostage->getTrack();
	automationlane_snapshot_t ref = toRef();
	parameter_ref_t p = {track->idx,  ref.type, 0, idx};
	DawInstance::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
}

track_params_t::track_params_t(audio_stage_t* _audiostage) : automatable_t(), audiostage(_audiostage) {
	const std::array<track_param_entry_t, 2> parameterTypes {{
		{ PARAM_ENABLE, "Enabled", 1.0f },
		{ PARAM_TRACK_GAIN, "Gain", dsp_util::gainToLinScale(1.0f) },
	}};
	for (const track_param_entry_t& paramEntry : parameterTypes) {
		automatable_param_t* regparam = registerParam(paramEntry.id);
		regparam->value = paramEntry.val;
		regparam->label = paramEntry.name;
		regparam->shortLabel = paramEntry.name;
	}
	for (int i = 0; i < MAX_SEND_CHANNELS; i++) {
		automatable_param_t* regparam = registerParam(PARAM_OFFSET_SEND+i);
		regparam->value = 0.0f;
		regparam->label = StringFormat("Send %d", (i+1));
		regparam->shortLabel = regparam->label;
	}
	getOrCreateAutomation(PARAM_ENABLE)->quantizationSteps = 1;
}

float track_params_t::getParamValue(int32_t idx) {
	automatable_param_t* param = getParam(idx);
	dbgassert(param);
	//		return convertValFrom(idx, param->value);
	return param->value;
}

void track_params_t::setParamValue(int32_t idx, float val, int flags) {
	automatable_param_t* param = getParam(idx);
	dbgassert(param);
	param->value = val; //convertValTo(idx, val);
}

track_t* track_params_t::getTrack() {
	return audiostage->getTrack();
}
const char* trackTypeNames[5] = {
	"Master", "Return", "Midi", "Audio", NULL
};
const char* TrackTypeToName(int type) {
	return trackTypeNames[type];
}

//vFILE_TYPES_TRACKSNAPSHOT
//const SupportedFileType FILE_TYPE_TRACKSNAPSHOT;

const SupportedFileType FILE_TYPE_TRACKSNAPSHOT {"Track File", "tracks"};
const std::vector<SupportedFileType> vFILE_TYPES_TRACKSNAPSHOT = { FILE_TYPE_TRACKSNAPSHOT };

bool storePluginPresetWithSnapshot = true;
bool loadPluginPresetWithSnapshot = false;
