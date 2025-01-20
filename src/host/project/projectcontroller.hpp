#pragma once
#include "fileio.hpp"
#include "project.hpp"
#include "tls.hpp"
#include "host/track/track.hpp"
#include "host/host_pluginmanager.hpp"

class project_controller_t {

    project_t* project;
    project_globals_t* projectGlobals;
    export_settings_t exportSettings;
    quantize_settings quantizeSettings;
protected:
    String projectPath;
public:
    project_controller_t(project_t* const _project, project_globals_t* const _projectGlobals)
        : project(_project), projectGlobals(_projectGlobals) {
    }
    virtual ~project_controller_t() = default;
    tick_t& getCursorPos() {
        return projectGlobals->cursor.cursorPos;
    }
    float getCurrentTempoBPM() {
        return projectGlobals->tempo100 / 100.0f;
    }
    int32_t getCurrentTempo() {
        return projectGlobals->tempo100;
    }
    virtual void setTempo(int32_t _tempo100) {
        projectGlobals->tempo100 = _tempo100;
    }
    tick_t& getPlaybackPos() {
        return projectGlobals->playbackPos;
    }
    tick_t& getIdleTickPos() {
        return projectGlobals->idleTickPos;
    }
    uint32_t sigNum() {
        return projectGlobals->signatureNum;
    }
    uint32_t sigDen() {
        return 1 << projectGlobals->signatureDenom;
    }
    uint32_t sigDenExp() {
        return projectGlobals->signatureDenom;
    }
    void setNum(uint32_t n) {
        projectGlobals->signatureNum = CLAMP_I(n, 1, 32);
    }
    void setDen(uint32_t d) {
        for (uint32_t i = 0; i <= 4; i++) {
            if (d < (1u << (i + 1u))) {
                projectGlobals->signatureDenom = i;
                return;
            }
        }
    }
    beatbar16th_t toBeatBar16th(tick_t tick, bool isRelative);
    tick_t beatBarNthToTick(const beatbar16th_t& beatBarNth, bool isRelative);

    static project_controller_t* get();
    double getProjectWorkingArea() {
        return 6000.0;
    }
    virtual void addTrackImpl(int32_t trackInsertPos, track_t* newTrack, int flags, std::optional<audio_stage_id_t> stageId = std::nullopt) {
        project->trackList.addTrack(trackInsertPos, newTrack);
        if ((flags & FLG_TRK_CHANGE_HISTORY_UNDO) != 0) {
            dbgassert(newTrack->audio);
        } else {
            dbgassert(!newTrack->audio);
            daw_tls::getTls().pluginManager->createAudio(newTrack, stageId);
        }
    }
    project_globals_t& getGlobals() {
        return *projectGlobals;
    }
    export_settings_t& getExportSettings() {
        return exportSettings;
    }
    quantize_settings& getQuantizeSettings() {
        return quantizeSettings;
    }
    trackallcontainer_t& getTracks() {
        return project->trackList;
    }
    std::vector<groove_data_t>& getGrooves() {
        return projectGlobals->grooveData;
    }
    const groove_data_t& getGrooveData(int32_t idx) {
        static groove_data_t empty{};
        if (idx < 0 || idx >= int32_t(projectGlobals->grooveData.size())) {
            return empty;
        }
        return projectGlobals->grooveData[idx];
    }
    project_t* getProject() {
        return project;
    }
    String getProjectName() {
        String name;
        SplitPath(projectPath, nullptr, &name, nullptr);
        return name;
    }
    String getProjectDirectory() {
        String dir;
        SplitPath(projectPath, &dir, nullptr, nullptr);
        return dir;
    }
    String getProjectPath() {
        return projectPath;
    }

};
