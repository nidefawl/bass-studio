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


const tick_t INVALID_TICK = 1 << 31;

void releaseClipResources(clip_t* cl, delete_cb *cb) {
	if (cb)
		cb->preClipDelete(cl);
	if (waveformrender::getInstance()) {
		waveformrender::getInstance()->assertWaveformRefIsUnbound(&cl->audio.waveformRef);
	}
	gui_clip* gClip = cl->gClip;
	if (gClip) {
		assert(gClip->parent);
		gClip->m_track->content->remove(gClip);
		DELETE_PTR(gClip);
	}
}
void releaseTrackResources(track_t* tr, delete_cb *cb) {
	assert(tr && tr->audio);
	if (cb)
		cb->preTrackDelete(tr);
	vsthost* host = vsthost::getInstance();
	host->unloadTrack(tr);
	tr->getMidi().deleteClips(cb);
	if (tr->mixer) {
		delete (tr->mixer);
	}
	if (tr->content) {
		delete (tr->content);
	}
	if (tr->subtracks.size()) {
		for (gui_track_subtrack* al : tr->subtracks) {
			delete al;
		}
		tr->subtracks.clear();
	}
	host->releaseAudio(tr);
	assert(tr && !tr->audio);
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
	assert(midi.getConstClips().empty());
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
  : tracksettings_t(a), localIdx(a.localIdx) {
	assert(midi.getConstClips().empty());
	for (const clip_t& clip : a.clips) {
		midi.addClip(new clip_t(clip));
	}
	assert(this->mixer == NULL);
	assert(this->content == NULL);
}
track_impl_snapshot_t::track_impl_snapshot_t(track_impl_t* p, bool storePluginChunks) {
	if (p) {
		p->arp->createSnapshot(trackArp);
		p->mixer.createSnapshot(trackParams);
		std::vector<effectbase*> effects = p->effects;
		pluginSnapshots.reserve(p->effects.size());
		for (effectbase* effect : p->effects) {
			plugin_snapshot_t ps;
			effect->makeSnapshot(ps, storePluginChunks);
			pluginSnapshots.push_back(std::move(ps));
		}
	}
}
track_snapshot_t::track_snapshot_t(track_t* track, bool storePluginChunks)
  : tracksettings_t(*track), localIdx(track->localIdx), plugins(track->audio, storePluginChunks)
{
	auto& otherClips = track->getMidi().getConstClips();
	for (auto clip : otherClips) {
		clips.emplace_back(*clip);
	}
	track_impl_t* p = track->audio;
	if (p) {
		p->saveSubtrackLayout(automationLanes);
	}
}


void track_t::loadSnapshot(const track_snapshot_t& snapshot) {
	auto audio = this->audio;
	assert(audio);
	const auto& implSnapshot = snapshot.plugins;
	audio->mixer.loadSnapshot(implSnapshot.trackParams);
	audio->arp->loadSnapshot(implSnapshot.trackArp);
	const std::vector<plugin_snapshot_t>& trPluginList = implSnapshot.pluginSnapshots;
	audio->loadPlugins(trPluginList);
	const std::vector<automationlane_snapshot_t>& atl = snapshot.automationLanes;
	this->subtracks.clear();
	bool showSubtracks = !this->hideSubtracks && !this->hideTrack;
	if (!showSubtracks) {
		audio->atl = atl;
		audio->wasInHide = true;
	} else {
		audio->wasInHide = false;
		audio->atl.clear();
		audio->loadSubtrackLayout(atl);
	}
}
void track_t::loadPluginAutomationParameters(const track_impl_snapshot_t& trackStatic) {
	assert(audio);
	const std::vector<plugin_snapshot_t>& trPluginList = trackStatic.pluginSnapshots;
	for (const plugin_snapshot_t& pluginSnapshot : trPluginList) {
		effectbase* effect = audio->getPluginById(pluginSnapshot.projectGlobalId);
		if (effect) {
			const std::vector<automation_view_t>& automatedParams = pluginSnapshot.automatedParams;
			for (const automation_view_t& automatedParam : automatedParams) {
				if (effect->hasParam(automatedParam.targetParam)) {
					automation_t* autom = effect->getAutomation(automatedParam.targetParam);
					autom->points = automatedParam.points;
				}
			}
		}
	}

}
void track_t::releaseTrackContent() {
}
void trackdata_midi_t::deleteClips(delete_cb *cb) {
	std::vector<clip_t*>::iterator it = clips.begin();
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
	return {this->id};
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
//	if (track->audio->selectedAutomationCtr == _effect) {
		track->audio->selectedAutomationCtr = NULL;
//	}
	audio_stage_t::removePlugin(_effect, notifyUp);
}
void audio_stage_t::removePlugin(effectbase* _effect, bool notifyUp) {
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
	assert(idx >= 0 && idx < (int32_t)effects.size());
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
		assert(idx < maxEvents);
		VstMidiEvent& evt = evtArr[idx];
		evt.type = kVstMidiType;
		evt.byteSize = sizeof(VstMidiEvent);
		evt.flags = 0;//kVstMidiEventIsRealtime;
		evt.deltaFrames = floor(nevt.tickOffsetInBlock*tickToSamples);
		assert(evt.deltaFrames >= 0 && evt.deltaFrames < blockSize);
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
int32_t audio_stage_t::getLatency() {
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
void track_impl_t::saveSubtrackLayout(std::vector<automationlane_snapshot_t>& atls)
{
	atls.reserve(track->subtracks.size());
	for (gui_track_subtrack* atl : track->subtracks) {
		automationlane_snapshot_t ref;
		if (atl->subtrackType() == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
			assert(atl->at);
			ref = atl->at->toRef();
		} else {
		}
		ref.paramIdx = atl->param;
		ref.height = atl->height;
		ref.subtrackType = atl->subtrackType();
		log_printf("save ref.type %d, ref.refId %d, ref.paramIdx %d\n", ref.type, ref.refId, ref.paramIdx);
		atls.push_back(std::move(ref));
	}

}
void track_impl_t::updateStoreLoadSubtracks() {
	bool hide = track->hideSubtracks || track->hideTrack;
	if (this->wasInHide == hide)
		return;
	this->wasInHide = hide;
	if (hide) {
		atl.clear();
		saveSubtrackLayout(atl);
		MainCtrl::getGuiTrackCtr()->removeAllSubtracks(track);
		Cursor& cursor = project_controller_t::get()->cursor;
		if (cursor.inSubTrackAny(track->idx)) {
			fixCursorSubRange(cursor, 0);
		}
	} else {
		loadSubtrackLayout(atl);
	}
}

effectbase* loadEffectModule(const plugin_snapshot_t& pluginSnapshot) {
	vsthost* host = vsthost::getInstance();
	String path;
	effectbase* effect = nullptr;
	vstplugin* loadedPlugin = nullptr;
	if (pluginSnapshot.pluginType == PLUGIN_TYPE_VST) {
		plugindatabase_t* db = plugindatabase_t::getInstance();
		if (db->resolve(pluginSnapshot.name, pluginSnapshot.uId, &path)) {
			vstpluginloadres res = host->loadPlugin(path, pluginSnapshot.projectGlobalId);
			if (res.result==0&&res.plugin) {
				loadedPlugin = res.plugin;
				effect = res.plugin;
			}
		} else {
			//TODO: handle failed plugin loading
			my_printf("Failed loading plugin %s, uId %d\n", StringAsCStr(pluginSnapshot.name), pluginSnapshot.uId);

		}
	} else {
		effect = host->makeModuleInstance(pluginSnapshot.pluginType, pluginSnapshot.uId, pluginSnapshot.projectGlobalId);
		if (effect && effect->getModuleType() == PLUGIN_TYPE_INTERNAL_EFFECT) {
			loadedPlugin = dynamic_cast<vstplugin*>(effect);
		}
	}
	if (loadedPlugin) {
		if (pluginSnapshot.dataChunk.size() > 0) {
			my_printf("Plugin %s: Load data1[%d]\n", StringAsCStr(loadedPlugin->sName), pluginSnapshot.dataChunk.size());
			loadedPlugin->dispatch(effSetChunk, 0, pluginSnapshot.dataChunk.size(), (void*)pluginSnapshot.dataChunk.data());
		}
		if (pluginSnapshot.dataChunk2.size() > 0) {
			my_printf("Plugin %s: Load data2[%d]\n", StringAsCStr(loadedPlugin->sName), pluginSnapshot.dataChunk2.size());
			loadedPlugin->dispatch(effSetChunk, 1, pluginSnapshot.dataChunk2.size(), (void*)pluginSnapshot.dataChunk2.data());
		}
	}
	return effect;
}
void loadEffectParamsFromSnapshot(const plugin_snapshot_t& pluginSnapshot, effectbase* effect) {

	const std::vector<param_snapshot_t>& pluginSnapshotParams = pluginSnapshot.params;
	for (const param_snapshot_t& param : pluginSnapshotParams) {
		int32_t paramIdxEffect = effect->mixerParams.size() + param.idx;
		if (effect->hasParam(paramIdxEffect)) {
			effect->setParamValue(paramIdxEffect, param.val, 1);
		}
	}
	const std::vector<param_snapshot_t>& pluginHostSideParams = pluginSnapshot.hostParams;
	for (const param_snapshot_t& param : pluginHostSideParams) {
		if (param.idx < (int32_t)effect->mixerParams.size() && effect->hasParam(param.idx)) {
			effect->setParamValue(param.idx, param.val, 1);
		}
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
			assert(effect->trackImpl == this);
			assert(effects.size());
		}
	}


}
void vsthost::activateDeferred(effectbase* eff) {
	assert(eff->trackImpl);
	assert(eff->trackImpl->effects.size());
	assert(eff->getSlot() >= 0);
	auto defEffect = dynamic_cast<effect_deferred*>(eff);
	plugin_snapshot_t pluginSnapshot = defEffect->getSnapshot();
	effectbase* effect = loadEffectModule(pluginSnapshot);
	if (!effect) {
		log_printf("Failed loading %s\n", StringAsCStr(pluginSnapshot.name));
//		assert(0);
		return;
	}
	loadEffectParamsFromSnapshot(pluginSnapshot, effect);
	effectbase* prevPlugin = nullptr;
	assert(removeEntry(eff->trackImpl->deferredEffects, eff));
	replacePlugin(eff->trackImpl, effect, defEffect->getSlot(), &prevPlugin);
	assert(removeEntry(this->pluginsDeferred, eff));
	effect->loadSnapshot(pluginSnapshot);
	loadAutomation(pluginSnapshot.automatedParams, effect);
	if (pluginSnapshot.enabled) {
		effect->resume();
	}

}
void track_impl_t::loadSubtrackLayout(const std::vector<automationlane_snapshot_t>& atls)
{
	guictr_tracks* guiTracks = MainCtrl::getGuiTrackCtr();
	if (guiTracks) {
		for (const automationlane_snapshot_t& ref : atls) {
			gui_track_subtrack* al = NULL;
			if (ref.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
				log_printf("loading ref.type %d, ref.refId %d, ref.paramIdx %d\n", ref.type, ref.refId, ref.paramIdx);
				if (ref.type == AUTOMATABLE_EFFECT) {
					effectbase* plugin = getPluginById(ref.refId);
					if (!plugin) {
						continue;
					}
					al = guiTracks->addAutomationLane(track, plugin, ref.paramIdx, false);

				}
				if (ref.type == AUTOMATABLE_MIXER) {
					al = guiTracks->addAutomationLane(track, &mixer, ref.paramIdx, false);
				}
				if (ref.type == AUTOMATABLE_ARP) {
					al = guiTracks->addAutomationLane(track, arp, ref.paramIdx, false);
				}
			} else if (ref.subtrackType == gui_track_subtrack::SUBTRACK_TYPE_AUTOMATION) {
//				al = new gui
			}
			if (al)
				al->height = ref.height;
		}
	}
}
void audio_stage_t::onTick(double since) {
	meter.onTick(since);
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
//	assert(0); // to be expected when deleting effectgroups
	return nullptr;
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
		int32_t clipStartSample = tickToSample(clipStartTick, bpm100, sampleRate);
		int32_t clipEndSample = tickToSample(clipEndTick, bpm100, sampleRate);
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

		cachedaudio_t* audio = audiocache::getInstance()->get(clip->audio.id);
		if (audio) {
			audiosample_t* sample = audio->sample.get();
			if (srcStartOffset >= (int32_t)sample->nSamples)
				continue;
			assert(sample->samples.size() > 0);
			for (int i = 0; i < 2; i++) {
				float *dst = buffer[i];
				auto& srcVector = i >= (int)sample->samples.size() ? sample->samples[sample->samples.size()-1] : sample->samples[i];
				int32_t len = math::min((int32_t)blockSize-math::max(0, -srcStartOffset),
								math::min(clipEndSampleLen, math::min(clipStartSampleLen, (int32_t)srcVector.size()-srcStartOffset)));
				assert(len>=0);
				if (len <= 0) { //TODO: could figure this out outside the loop
					continue;
				}
				assert(dstStartOffset+len <= (int32_t)blockSize);
				assert(srcStartOffset+len <= (int32_t)srcVector.size());
				assert(dstStartOffset>=0);
				memcpy(dst+dstStartOffset, srcVector.data()+math::max(0, srcStartOffset), len*sizeof(float));
			}
		}
	}
}
void sortNoteEvents(std::vector<noteevent_t>& noteEvents) {
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
}
track_impl_t::track_impl_t(int32_t _id, track_t* _track, const samplerate_t& _sampleRate, const uint16_t& _blockSize, int32_t nChannels)
   : audio_stage_t(_id, /*_track, */_sampleRate, _blockSize, nChannels, 0)
  , track(_track)
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
void track_impl_t::sendNotesOff(int32_t bpm100, int32_t blockSamplePos) {
	VstEvent_t* midiEventsBuf = reallocEvts(track->audio->heldNotes.size());
	for (note_t& note : track->audio->heldNotes) {
//		midiEventsBuf->writeVstMidiEvt(note, bpm100, blockSamplePos, sampleRate, blockSize, false);
		my_printf("Send note off %d\n", note.pitch);
	}
	if (arp)
		arp->allNotesOff();
	midiEventsBuf->writeInstantOff();
	track->audio->heldNotes.clear();
	for (effectbase* effect : effects) {
		vstplugin* vst = dynamic_cast<vstplugin*>(effect);
		if (vst && vst->bCanReceiveMidi) {
			//			VstEvent_t midiEventsBufTemp = *midiEventsBuf; // make a copy
			vst->dispatch(effProcessEvents, 0, 0, midiEventsBuf->vstEvents);
		}
	}
}
void track_impl_t::sendNotes(tick_t start, tick_t end, tick_t loopStart, tick_t loopEnd, int32_t bpm100, int32_t blockSamplePos) {
	//assert(end != loopEnd); //if end equals loopEnd note off events will be on exact end
	if (std::any_of(effects.begin(), effects.end(), [](const effectbase* ref){
			return ref->bCanReceiveMidi;
	})) {
		std::vector<note_t> notes;

		tick_t heldBegin = start;
		tick_t heldEnd = end;
		for (const note_t& note : heldNotes) {
			heldBegin = math::min(heldBegin, note.start());
			heldEnd = math::max(heldEnd, note.end());
		}
		track->getMidi().getNotesInRange(heldBegin, heldEnd, -1, loopEnd, notes);
		if (loopStart > -1&&start<loopStart)
			start = loopStart;
		if (!notes.empty() || !heldNotes.empty() || arp != nullptr) {
			std::vector<note_t> notesBegin;
			std::vector<note_t> notesEnd;
			std::vector<noteevent_t> noteEvents;
			notesBegin.reserve(notes.size());
			notesEnd.reserve(heldNotes.size()+6);
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
//			for (note_t& note : heldNotes) {
//				if (note.end() >= start && note.end() < end) {
//					notesEnd.push_back(note);
//					noteEvents.emplace_back(note, false);
//					my_printf("%d END NOTE %d %d\n", blockSamplePos/blockSize, note.time, note.pitch);
//				}
//			}
			addAll(heldNotes, notesBegin);
			removeAll(heldNotes, notesEnd);
			sortNoteEvents(noteEvents);
//			if (noteEvents.size()) {
//				my_printf("%d %d NOTES\n", start, noteEvents.size());
//			}
//			for (noteevent_t& note : noteEvents) {
//				my_printf("NOTE_%s %d %d\n", note.isNoteOn?"ON":"OFF", note.tickOffsetInBlock, note.pitch);
//			}
			std::vector<noteevent_t> noteEventsProcessed;
			arp->process(noteEvents, start, end, loopStart, loopEnd, noteEventsProcessed);
			size_t numEvents = noteEventsProcessed.size();
			VstEvent_t* midiEventsBuf = reallocEvts(numEvents);
			const double ticksPerBlock = toTickPrecise(blockSize/(double)sampleRate, bpm100);
			const double tickToSamples = (60*sampleRate) / (bpm100/100.0*TICKS_QUARTER);
			for (noteevent_t& evt : noteEventsProcessed) {
				assert(evt.tickOffsetInBlock >= 0 && evt.tickOffsetInBlock < ticksPerBlock);
				midiEventsBuf->writeVstMidiEvt(evt, tickToSamples, blockSize);
			}
			assert(midiEventsBuf->vstEvents->numEvents == (int32_t) numEvents);
			for (effectbase* effect : effects) {
				vstplugin* vst = dynamic_cast<vstplugin*>(effect);
				if (vst && vst->bCanReceiveMidi) {
//					VstEvent_t midiEventsBufTemp = *midiEventsBuf; // make a copy
					vst->dispatch(effProcessEvents, 0, 0, midiEventsBuf->vstEvents);
				}
			}
		} else {
			for (effectbase* effect : effects) {
				vstplugin* vst = dynamic_cast<vstplugin*>(effect);
				if (vst && vst->bCanReceiveMidi) {
					static VstEvents noEvData;
					noEvData = {  };
					vst->dispatch(effProcessEvents, 0, 0, &noEvData);
				}
			}
		}
	}

//	return NULL;

}

void track_params_t::createSnapshot(track_params_snapshot_t& snapshot) {
	for (int i = 0; i < getNumParameters(); i++) {
		float val = params[i].value;
		param_snapshot_t snapParam{i, val};
		snapshot.params.push_back(std::move(snapParam));
		automation_t* automation = getAutomation(i);
		assert(automation);
//		automation_view_t automationView;
//		if (automation) {
//			automationView.targetParam = i;
//			automationView.points = automation->points;
//			automationView.active = automation->active;
//		}
//		snapshot.automatedParams.push_back(std::move(automationView));
	}
	storeAutomation(snapshot.automatedParams, this);
}
void track_params_t::loadSnapshot(const track_params_snapshot_t& snapshot) {
	for (auto p : snapshot.params) {
		params[p.idx].value = p.val;
	}
//	for (auto p : snapshot.automatedParams) {
//		automation_t* automation = getAutomation(p.targetParam);
//		automation->points = p.points;
//		automation->active = p.active;
//	}
	loadAutomation(snapshot.automatedParams, this);
}
void track_params_t::postSetParameter(int32_t idx, float preVal, float val, int flags) {
	if (flags != 2) {
		return;
	}
	assert(this->audiostage->getTrack());
	track_t* track = this->audiostage->getTrack();
	automationlane_snapshot_t ref = toRef();
	parameter_ref_t p = {track->idx,  ref.type, 0, idx};
	MainCtrl::get()->pushHist(new action_modify_effect_parameter("Modify parameter", p, preVal, val));
}
const char* trackTypeNames[5] = {
	"Master", "Return", "Midi", "Audio", NULL
};
const char* TrackTypeToName(int type) {
	return trackTypeNames[type];
}
