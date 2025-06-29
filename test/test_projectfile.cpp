#include "TestBase.hpp"
#include "buildinfo.h"
#include "fileio.hpp"
#include "logging.hpp"
#include "platform.hpp"
#include "file/groovefile.hpp"
#include "file/projectfile-v1.hpp"
#include "file/projectfile-v2.hpp"
#include "snapshot/plugin-snapshot.hpp"
#include "snapshot/track-snapshot.hpp"

namespace {
    void test_projectfile_v1() {
        TEST_BEGIN("test_projectfile_v1");
        String testOldFormat = TEST_PATH("projects/legacy.project");
        auto res             = DAW::ProjectFileV1::loadProjectFromJsonFile(testOldFormat);
        if (std::holds_alternative<String>(res)) {
            log_printf("Failed to load project file %s: %s\n", StringAsCStr(testOldFormat), StringAsCStr(std::get<String>(res)));
            return;
        }
        auto projectFile = std::get<std::shared_ptr<project_file>>(res);
        TEST_END();

        TEST_BEGIN("test_projectfile_v1_save");
        auto outFile = TEST_PATH("projects/legacy.project.temp");
        auto resSave = DAW::ProjectFileV1::saveProjectToJsonFile(projectFile, outFile);
        if (resSave) {
            log_printf("Failed to save project file %s: %s\n", outFile, StringAsCStr(resSave.value()));
        }
        TEST_ASSERT_THROW(!resSave);
        DeleteAbsoluteFile(outFile);
        TEST_END();
    }

    void test_projectfile_v2() {
        TEST_BEGIN("test_projectfile_v2_load");
        String testNewFormat = TEST_PATH("projects/empty-project.bsp");
        auto res             = DAW::ProjectFileV2::loadProjectFromJsonFile(testNewFormat);
        if (std::holds_alternative<String>(res)) {
            log_lf(Log::L_ERROR, "Failed to load project file %s: %s\n", StringAsCStr(testNewFormat), StringAsCStr(std::get<String>(res)));
            return;
        }
        TEST_ASSERT_EQUAL(std::holds_alternative<String>(res), false);
        auto projectFile = std::get<std::shared_ptr<project_file>>(res);
        TEST_ASSERT_EQUAL(projectFile->project.trackCtr.tracks.size(), 1UL);
        TEST_ASSERT_EQUAL(projectFile->project.trackReturnCtr.tracks.size(), 0UL);
        TEST_ASSERT_EQUAL(projectFile->project.trackMasterCtr.tracks.size(), 1UL);
        TEST_END();

        TEST_BEGIN("test_projectfile_v2_save");
        auto outFile = TEST_PATH("projects/empty-project.bsp.temp");
        auto resSave = DAW::ProjectFileV2::saveProjectToJsonFile(projectFile, outFile);
        if (resSave) {
            log_printf("Failed to save project file %s: %s\n", outFile, StringAsCStr(resSave.value()));
        }
        TEST_ASSERT_THROW(!resSave);
        DeleteAbsoluteFile(outFile);
        TEST_END();

        TEST_BEGIN("test_projectfile_v2_missing");
        testNewFormat = TEST_PATH("projects/missing.bsp");
        res           = DAW::ProjectFileV2::loadProjectFromJsonFile(testNewFormat);
        TEST_ASSERT_EQUAL(std::holds_alternative<String>(res), true);
        TEST_END();
    }

    void test_preset_file() {
        TEST_BEGIN("test_preset_file_load");
        String testPreset = TEST_PATH("presets/triangle.preset");
        auto res = DAW::ProjectFileV2::loadPluginSnapshot(testPreset);
        if (std::holds_alternative<String>(res)) {
            log_lf(Log::L_ERROR, "Failed to load preset file %s: %s\n", StringAsCStr(testPreset), StringAsCStr(std::get<String>(res)));
            return;
        }
        TEST_ASSERT_EQUAL(std::holds_alternative<String>(res), false);
        auto snapshot = std::get<std::shared_ptr<plugin_snapshot_t>>(res);
        TEST_ASSERT_THROW(snapshot->version >= 18);
        TEST_END();

        TEST_BEGIN("test_preset_file_save");
        auto outFile = TEST_PATH("presets/triangle.preset.temp");
        auto resSave = DAW::ProjectFileV2::savePluginSnapshot(*snapshot, outFile);
        if (resSave) {
            log_printf("Failed to save preset file %s: %s\n", outFile, StringAsCStr(resSave.value()));
        }
        TEST_ASSERT_THROW(!resSave);
        DeleteAbsoluteFile(outFile);
        TEST_END();

        TEST_BEGIN("test_preset_file_missing");
        testPreset = TEST_PATH("presets/missing.preset");
        res        = DAW::ProjectFileV2::loadPluginSnapshot(testPreset);
        TEST_ASSERT_EQUAL(std::holds_alternative<String>(res), true);
        TEST_END();
    }

    void test_trackcontainer_file() {
        TEST_BEGIN("test_trackcontainer_file_load");
        String testTrackContainer = TEST_PATH("tracks/synth-shaper.tracks");
        auto res                  = DAW::ProjectFileV2::loadTrackContainer(testTrackContainer);
        if (std::holds_alternative<String>(res)) {
            log_lf(Log::L_ERROR, "Failed to load track container file %s: %s\n", StringAsCStr(testTrackContainer), StringAsCStr(std::get<String>(res)));
            return;
        }
        TEST_ASSERT_EQUAL(std::holds_alternative<String>(res), false);
        auto snapshot = std::get<std::shared_ptr<trackcontainer_snapshot_t>>(res);
        TEST_ASSERT_EQUAL(snapshot->version, 2);
        TEST_END();

        TEST_BEGIN("test_trackcontainer_file_save");
        auto outFile = TEST_PATH("tracks/synth-shaper.tracks.temp");
        auto resSave = DAW::ProjectFileV2::saveTrackContainer(*snapshot, outFile);
        if (resSave) {
            log_printf("Failed to save track container file %s: %s\n", outFile, StringAsCStr(resSave.value()));
        }
        TEST_ASSERT_THROW(!resSave);
        DeleteAbsoluteFile(outFile);
        TEST_END();

        TEST_BEGIN("test_trackcontainer_file_missing");
        testTrackContainer = TEST_PATH("tracks/missing.tracks");
        res                = DAW::ProjectFileV2::loadTrackContainer(testTrackContainer);
        TEST_ASSERT_EQUAL(std::holds_alternative<String>(res), true);
        TEST_END();
    }

    void test_groove_file() {
        TEST_BEGIN("test_groove_file_load");
        String testGroove = TEST_PATH("grooves/groovelibrary.groove");
        auto res = DAW::ProjectFileV2::loadGrooveFile(testGroove);
        if (std::holds_alternative<String>(res)) {
            log_lf(Log::L_ERROR, "Failed to load groove file %s: %s\n", StringAsCStr(testGroove), StringAsCStr(std::get<String>(res)));
            return;
        }
        TEST_ASSERT_EQUAL(std::holds_alternative<String>(res), false);
        auto grooveFile = std::get<groove_file_t>(res);
        TEST_ASSERT_EQUAL(grooveFile.version, 2);
        TEST_END();

        TEST_BEGIN("test_groove_file_save");
        auto outFile = TEST_PATH("grooves/groovelibrary.groove.temp");
        auto resSave = DAW::ProjectFileV2::saveGrooveFile(grooveFile, outFile);
        if (resSave) {
            log_printf("Failed to save groove file %s: %s\n", outFile, StringAsCStr(resSave.value()));
        }
        TEST_ASSERT_THROW(!resSave);
        DeleteAbsoluteFile(outFile);
        TEST_END();

        TEST_BEGIN("test_groove_file_missing");
        testGroove = TEST_PATH("grooves/missing.groove");
        res        = DAW::ProjectFileV2::loadGrooveFile(testGroove);
        TEST_ASSERT_EQUAL(std::holds_alternative<String>(res), true);
        TEST_END();
    }
}// namespace

int main() {
    App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    test_projectfile_v1();
    test_projectfile_v2();
    test_preset_file();
    test_trackcontainer_file();
    test_groove_file();
    return 0;
}
