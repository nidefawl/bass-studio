#pragma once
#include <memory>
#include <map>
#include "assert_dbg.h"
#include "cursor.h"
#include "daw_async_task.h"
#include "host/audiobuffer/audioblock.h"
#include "host/audiocache/audiocache.h"
#include "host/clip/clip.h"
#include "host/daw/clipboard.h"
#include "gui/track/trackctr.h"
#include "host/daw/mainctrl.h"
#include "host/track/track_impl.h"
#include "host/track/track_types.h"
#include "types.h"

namespace DAW {


inline clip_t* ConsolidateAudioClips(DawInstance* daw, track_t* track, tick_t tickBegin, tick_t tickEnd, const String& name) {
    auto& prjGlobals = daw->getProjectGlobals();
    auto& trackData = track->getMidi();
    auto clips = trackData.getConstClips();

    create_sample_req_t csr;
    csr.format      = track->audio->sampleFormat;
    csr.id          = -1;
    csr.numChannels = track->audio->input.channels;
    auto [fPath, fName] = daw->createUniqueNonExistingFilename("samples", track->name, name, "wav");
    csr.path = fPath;

    auto numSamples = tickToSampleConvert<samplecount_t, roundmode::ceil>(tickEnd - tickBegin, prjGlobals.tempo100, csr.format.sampleRate);
    auto cache = daw->getAudioCache();
    //TODO: createSample is not thread safe, we might be doing a lookup from waveformrenderer
    auto samplefile = cache->createSample(csr);
    store_sample_req_t ssr;
    ssr.id = samplefile->id;
    ssr.format = csr.format;
    ssr.offset = 0;
    ssr.length = numSamples;
    ssr.channels.resize(csr.numChannels);
    for (auto& ch : ssr.channels) {
        ch.resize(numSamples);
    }
    samplecount_t blockSize = 512;
    auto blockStoreSampleChannels = AudioBlock(ssr.channels);
    blockStoreSampleChannels.clear();

    // Copying can be done in one pass, but is done iteratively to allow for cancellation and progress
    samplecount_t nSamplesRead = 0;
    double ticksPerBlock = sampleToTickConvert<double, roundmode::none>(blockSize, prjGlobals.tempo100, csr.format.sampleRate);
    samplecount_t samplePosFromTick = tickToSampleConvert<samplecount_t, roundmode::floor>(tickBegin, prjGlobals.tempo100, ssr.format.sampleRate);
    double readTickBegin = tickBegin;
    while (nSamplesRead < numSamples) {
        double readTickEnd = readTickBegin + ticksPerBlock;
        auto readSamplesLeft = numSamples - nSamplesRead;
        auto numSamplesRead = math::min(blockSize, readSamplesLeft);
        if (readSamplesLeft < blockSize) {
            readTickEnd = sampleToTickConvert<double, roundmode::none>(readSamplesLeft, prjGlobals.tempo100, csr.format.sampleRate);
        }
        auto tempBlock = blockStoreSampleChannels.getOffsetBlock(nSamplesRead);
        track->audio->fillAudio(math::floordS32(readTickBegin), math::floordS32(readTickEnd), -1, -1, prjGlobals, samplePosFromTick, numSamplesRead, tempBlock);
        readTickBegin = readTickEnd;
        samplePosFromTick += blockSize;
        nSamplesRead += numSamplesRead;
    }
    cache->updateSample(ssr);



    auto audioClip = new clip_t{};
    audioClip->name = name.empty() ? fName : name;
    audioClip->time = tickBegin;
    audioClip->len = tickEnd - tickBegin;
    audioClip->loopStart = 0;
    audioClip->loopLen   = tickEnd - tickBegin;
    audioClip->clipType = CLIP_AUDIO;
    audioClip->audio.id = samplefile->id;
    audioClip->rgb = track->rgb;

    return audioClip;
}

    struct consolidate_fill_audio_t {
        DawInstance* const daw;
        track_t* const track;
        sampleformat_t format{};
        channelnum_t numChannels;
        int32_t sampleId = 0;
        tick_t windowBegin = 0;
        tick_t windowEnd = 0;
        samplecount_t blockSize = 2048;
        samplecount_t samplePos = 0;
        AudioBlock blockTemp;
        samplecount_t numSamplesRead = 0;
        samplecount_t numSamples = 0;
        project_globals_t prjGlobals;
        String name;
        String path;
        bool finished = false;
        store_sample_req_t ssr;
        consolidate_fill_audio_t(DawInstance* daw, track_t* track, tick_t tickBegin, tick_t tickEnd, const String& name)
            : daw(daw),
            track(track),
            format(track->audio->sampleFormat),
            numChannels(track->audio->input.channels),
            windowBegin(tickBegin),
            windowEnd(tickEnd),
            prjGlobals(daw->getProjectGlobals())
        {
            auto [fPath, fName] = daw->createUniqueNonExistingFilename("samples", track->name, name, "wav");
            this->name = fName;
            this->path = fPath;
            auto& prjGlobals = daw->getProjectGlobals();
            auto& trackData = track->getMidi();
            auto clips = trackData.getConstClips();
            samplePos = tickToSampleConvert<samplecount_t, roundmode::ceil>(tickBegin, prjGlobals.tempo100, format.sampleRate);
            numSamples = tickToSampleConvert<samplecount_t, roundmode::ceil>(tickEnd - tickBegin, prjGlobals.tempo100, format.sampleRate);

            create_sample_req_t csr;
            csr.format      = format;
            csr.id          = -1;
            csr.numChannels = numChannels;
            csr.path = this->name;
            //TODO: createSample is not thread safe, we might be doing a lookup from waveformrenderer
            auto cache = daw->getAudioCache();
            auto samplefile = cache->createSample(csr);
            dbgassert(samplefile);
            sampleId = samplefile->id;
            ssr.id = sampleId;
            ssr.format = format;
            ssr.channels.resize(numChannels);
            ssr.preAllocate = numSamples;
            for (auto& ch : ssr.channels) {
                ch.resize(blockSize);
            }
            blockTemp = AudioBlock(ssr.channels);
        }
        double getProgress() const {
            return numSamples ? (double)numSamplesRead / (double)numSamples : 0.0;
        }
        void copyStep() {
            if (finished) {
                return;
            }
            blockTemp.clear();
            auto readSamplesLeft = numSamples - numSamplesRead;
            auto readLen = math::min(blockSize, readSamplesLeft);
            track->audio->fillAudio(windowBegin, windowEnd, -1, -1, prjGlobals, samplePos, readLen, blockTemp);
            samplePos += readLen;
            ssr.offset = numSamplesRead;
            numSamplesRead += readLen;
            ssr.length = readLen;
            auto cache = daw->getAudioCache();
            cache->updateSample(ssr);
            if (numSamplesRead >= numSamples) {
                finished = true;
            }
        }
        bool isFinished() const {
            return finished;
        }
    };
struct consolidate_task_t : public async_task_t {
    DawCtrl* dawCtrl;
    DawInstance* daw;
    track_gui_manager_i* iGuiMgr;
    DAW::Cursor cursor;
    std::map<int32_t, std::array<int32_t, 2>> mapTrClCount;
    std::shared_ptr<clip_clipboard> clipboardCopy;
    std::shared_ptr<clip_clipboard> clipboardConsolidated;
    bool bCopyAutomation;
    int32_t numTracks = 0;
    int32_t currentTrack = 0;
    std::shared_ptr<consolidate_fill_audio_t> fillAudio;
    String progressDesc = "";
    String getTaskName() const override {
        return "Consolidate";
    }
    String getProgressDesc() const override {
        return progressDesc;
    }
    void run() override {
        switch (m_state) {
        case state::idle:
            copySelection();
            setRunning();
            progressDesc = StringFormat("Clip %d/%d", currentTrack+1, numTracks);
            break;
        case state::running:
            consolidateAudio();
            if (currentTrack >= this->numTracks) {
                pasteClipboard();
                setFinished();
            }
            break;
        case state::error:
        case state::finished:
        case state::cancelled:
            break;
        }
    }
    void getPreciseProgress(double& progressOverall, double& progressDetail) override {
        if (fillAudio) {
            progressDetail = fillAudio->getProgress();
        } else {
            progressDetail = 0;
        }
        progressOverall = numTracks ? ((currentTrack + progressDetail) / double(numTracks)) : 0.0;
    }
    void copySelection() {
        int32_t trackBegin    = cursor.getTrackBegin();
        int32_t trackEnd      = cursor.getTrackEnd();
        int32_t tickBegin    = cursor.getTickBegin();
        int32_t tickEnd      = cursor.getTickEnd();
        for (int32_t trIdx = trackBegin; trIdx <= trackEnd; trIdx++) {
            track_clipboard_t trackClipboard;
            if (iGuiMgr->validTrackIdx(trIdx)) {
                track_gui_entry_t* trEntry = iGuiMgr->atNC(trIdx);
                mapTrClCount[trIdx][CLIP_AUDIO] = 0;
                mapTrClCount[trIdx][CLIP_MIDI] = 0;
                auto& trackData = trEntry->track->getConstMidi();
                auto& constClips = trackData.getConstClips();
                for (const auto& c : constClips) {
                    if (c->clipType == CLIP_AUDIO || c->clipType == CLIP_MIDI) {
                        if (c->start() < tickEnd && c->end() > tickBegin) {
                            mapTrClCount[trIdx][c->clipType]++;
                        }
                    }
                }
            }
        }
        this->clipboardConsolidated = DAW::consolidateClipboard(clipboardCopy, cursor);
        this->numTracks = CtrSize(clipboardConsolidated->tracks);
    }
    void consolidateAudio() {
        int32_t trackBegin = cursor.getTrackBegin();
        int32_t trackCnt = 0;
        for (auto& [trIdx, count] : mapTrClCount) {
            if (trackCnt++ < currentTrack) {
                continue;
            }
            if (!assert_expr(iGuiMgr->validTrackIdx(trIdx))) {
                continue;
            }
            track_gui_entry_t* trEntry = iGuiMgr->atNC(trIdx);
            if (!assert_expr((trIdx - trackBegin) < CtrSize(clipboardConsolidated->tracks))) {
                continue;
            }
            auto clipType = trEntry->track->type == TRACK_TYPE_AUDIO ? CLIP_AUDIO : CLIP_MIDI;
            if (count[CLIP_AUDIO] || count[CLIP_MIDI]) {
                clipType = count[CLIP_AUDIO] > count[CLIP_MIDI] ? CLIP_AUDIO : CLIP_MIDI;
            }
            auto trClipboard = clipboardConsolidated->tracks[trIdx - trackBegin].get();
            if (!assert_expr(trClipboard->clips.size() == 1)) {
                continue;
            }
            if (trClipboard->clips.empty()) {
                continue;
            }
            auto* clip = trClipboard->clips[0].get();
            if (clip->name.empty()) {
                auto preClipboard = clipboardCopy->tracks[trIdx - trackBegin];
                if (!preClipboard->clips.empty()) {
                    clip->name = preClipboard->clips[0]->name;
                }
            }
            if (clipType == CLIP_MIDI) {
                clip->audio = {};
                clip->rgb = trEntry->track->rgb;
                clip->clipType = clipType;
                clip->setDirty();
                currentTrack = trackCnt;
                progressDesc = StringFormat("Clip %d/%d", currentTrack+1, numTracks);
            } else {
                //DawInstance* daw, track_t* track, tick_t tickBegin, tick_t tickEnd, const String& name
                if (!fillAudio) {
                    fillAudio = std::make_shared<consolidate_fill_audio_t>(daw, trEntry->track, cursor.getTickBegin(), cursor.getTickEnd(), clip->name);
                }
                fillAudio->copyStep();
                if (fillAudio->isFinished()) {
                    currentTrack = trackCnt;
                    progressDesc = StringFormat("Clip %d/%d", currentTrack+1, numTracks);
                    clip->name = clip->name.empty() ? fillAudio->name : clip->name;
                    clip->time = cursor.getTickBegin();
                    clip->len = cursor.getTickEnd() - cursor.getTickBegin();
                    clip->loopStart = 0;
                    clip->loopLen   = cursor.getTickEnd() - cursor.getTickBegin();
                    clip->clipType = CLIP_AUDIO;
                    clip->audio.id = fillAudio->sampleId;
                    clip->rgb = fillAudio->track->rgb;
                    clip->notes = {};
                    clip->rgb = trEntry->track->rgb;
                    clip->setDirty();
                    fillAudio = nullptr;
                }
                break;
            }
        }
    }
    void pasteClipboard() {
        trackstate_t preModifyState;
        int32_t idxBegin = iGuiMgr->getTrackProjectIndex(cursor.getTrackBegin());
        int32_t idxEnd   = iGuiMgr->getTrackProjectIndex(cursor.getTrackEnd());
        daw->getTracks().copyTracks(idxBegin, idxEnd, preModifyState);
        preModifyState.cursor = cursor;
        cursor.setLeftAligned();
        DAW::cutSelection(daw, *iGuiMgr, cursor, bCopyAutomation);
        DAW::pasteClipboard(daw, *iGuiMgr, clipboardConsolidated.get(), cursor, bCopyAutomation);
        dawCtrl->getGrid().makeTickVisible(cursor.getTickBegin()+cursor.getRange()/2);
        auto* track_action = new action_modify_track("Consolidate selection", preModifyState.copy());// could be more efficient
        daw->pushHist(track_action);
        daw->updateVisibleTrackContents();
    }
};

}
