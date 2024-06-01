#pragma once
#include "cursor.h"
#include "daw_async_task.h"
#include "gui/track/trackcontrols.h"
#include "host/clip/clip.h"
#include "host/daw/mainctrl.h"
#include "host/track/track_impl.h"
#include "host/track/track_types.h"

namespace DAW {

struct freeze_track_task_t final : public async_task_t {
    DawInstance* daw = nullptr;
    track_t* track = nullptr;
    track_t* trackTo = nullptr;
    DAW::Cursor cursor;
    export_settings_t exportSettings;
    export_settings_t exportSettingsRestore;
    bool bCacheAudioGraphRestore = false;
    String getTaskName() const override {
        return "Freeze track";
    }
    String getProgressDesc() const override {
        return "Freezing track";
    }
    void cloneTrack(track_gui_entry_t* entry) {
        daw->setAudioThreadState(playback_state::status_no_process);
        {
            auto lock = daw->lockPlayThread();
            auto trInsertPos = track->localIdxFlat + 1;
            trackTo = daw->insertNewTrack(trInsertPos, TRACK_TYPE_AUDIO);
            trackTo->name = track->name + " (frozen)";
            trackTo->rgb = track->rgb;
            gui_track_drop_position_t pos = {
                .slot = track->localIdxFlat + 1,
                .droppedTrack = entry,
                .droptype = gui_track_drop_position_t::drop_type::track_after,
                .pos = entry->mixer->getLeftBottom()
            };
            moveTrackToSlot(daw, trackTo, pos);

            auto trackStage = trackTo->getStage();
            trackStage->inputChannel = DAW::ChannelStage(track->getStage(), stage_bufferpoint::OUTPUT_POST, 0, 0);
            trackStage->outputChannel = DAW::ChannelNone();
            trackStage->flags |= audiostageflags_t::RECORD_FORCE;
            exportSettingsRestore = daw->getExportSettings();
            exportSettings.exportLen = cursor.getRange();
            exportSettings.exportPos = cursor.getTickBegin();
            exportSettings.exportPath = trackTo->name + "-frozen.wav";
            daw->getExportSettings() = exportSettings;
            bCacheAudioGraphRestore = daw->getHost()->cacheAudioGraph;
            std::shared_ptr<DAW::processing_graph_t> processingGraph;
            /* First build a full track graph. Then find trackTo and add it plus all its children to a new graph */
            auto project = daw->getProject();
            auto tracksFlatAll = project->trackList.getAllTracksFlatVec();//TODO: get rid of copy
            std::vector<track_t*> tracksSolo = { trackTo };
            if (!DAW::buildProcessingGraphSolo(daw->getHost(), project, tracksFlatAll, tracksSolo, processingGraph)) {
                log_lf(Log::L_ERROR, "Failed building track graph\n");
                m_state = state::error;
                finish();
            } else {
                daw->getHost()->setCustomGraph(processingGraph);
            }

        }

    }
    void run() override {
        switch (m_state) {
            case state::idle: {
                m_state = state::running;
                daw->startExport();
                break;
            }
            case state::running: {
                if (daw->getPlayThread()->getState() != playback_state::status_render) {
                    m_state = state::finished;
                }
                break;
            }
            default:
                break;
        }
        switch (m_state) {
            case state::error:
            case state::finished:
            case state::cancelled:
                finish();
                daw->getExportSettings() = exportSettingsRestore;
                break;
            default:
                break;
        }
        requestFrame();
    }
    void getPreciseProgress(double& progressOverall, double& progressDetail) override {
        progressDetail  = -1;
        auto tick = daw->getPlaybackPos() - exportSettings.exportPos;
        progressOverall = tick / double(exportSettings.exportLen);
    }
    void finish() {
        auto lock = daw->lockPlayThread();
        daw->getHost()->cacheAudioGraph = bCacheAudioGraphRestore;
        auto trackStage = trackTo->getStage();
        trackStage->inputChannel = DAW::ChannelDefaultNone();
        trackStage->outputChannel = DAW::ChannelDefaultNone();
        trackStage->recorder.finishRecordingClip();
        trackStage->recorder.writeRecordedData(daw, trackStage, daw->getAudioCache(), daw);
        trackStage->flags &= ~audiostageflags_t::RECORD_FORCE;
        m_state = canceled ? state::cancelled : state::finished;
        if (m_state == state::finished) {
            for (auto clip : trackTo->getClips().getClips()) {
                clip->len = exportSettings.exportLen;
            }
        }
        daw->updateVisibleTrackContents();
    }
    void cancel() override {
        canceled = true;
        daw->stopPlaying();
        finish();
    }
};
  
} // namespace DAW