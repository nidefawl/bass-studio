#include "TestBase.hpp"
#include "common/test_common.hpp"
#include "fileio.hpp"
#include "logging.hpp"
#include "platform.hpp"
#include "file/projectfile-v1.hpp"
#include "file/projectfile-v2.hpp"
#include "snapshot/track-snapshot.hpp"
#include "str_util.hpp"
#include "buildinfo.h"
#include <vector>
#include <memory>

namespace {
    void test_projectfile_loader() {
        TEST_BEGIN("test_projectfile_loader");
        std::vector<FileFound> projectFilesToTest;
        findFilesWithExt(TEST_PATH("projects"), PROJECT_FILE_EXT, true, projectFilesToTest);
        findFilesWithExt(TEST_PATH("project"), PROJECT_BUNDLE_FILE_EXT, true, projectFilesToTest);
        log_printf("projects: %zu files\n", projectFilesToTest.size());
        int64_t numSuccess  = 0;
        int64_t numFails    = 0;
        int64_t numNoTracks = 0;
        int64_t numLoaded   = 0;

        for (const FileFound& file : projectFilesToTest) {
            numLoaded++;
            String path = file.path;
            auto res = DAW::ProjectFileV2::loadProjectFromJsonFile(path);
            if (std::holds_alternative<String>(res)) {
                numFails++;
                log_lf(Log::L_ERROR, "Error: failed loading file %s: %s\n", StringAsCStr(path), StringAsCStr(std::get<String>(res)));
                continue;
            }
            std::shared_ptr<project_file> projectFile = std::get<std::shared_ptr<project_file>>(res);
            numSuccess++;
            size_t numTracks = 0;
            numTracks += projectFile->project.trackCtr.tracks.size();
            numTracks += projectFile->project.trackReturnCtr.tracks.size();
            numTracks += projectFile->project.trackMasterCtr.tracks.size();
            /* String outputPath = "projects_out/" + file.name;
            log_lf(Log::L_DEBUG, "Saving %s to %s\n", StringAsCStr(path), StringAsCStr(outputPath));
            saveProject(projectFile, outputPath); */
            projectFile.reset();
            if (!numTracks) {
                numNoTracks++;
            }
        }
        log_lf(Log::L_DEBUG, "%zd / %zu files failed to load\n", numFails, numLoaded);
        log_lf(Log::L_DEBUG, "%zd / %zu files had 0 tracks\n", numNoTracks, numSuccess);
        TEST_END();
    }
}// namespace



namespace {
    void test_projectfile_v2() {
        TEST_BEGIN("test_projectfile_v2");
        String testOldFormat = TEST_PATH("projects/legacy.project");
        auto res = DAW::ProjectFileV1::loadProjectFromJsonFile(testOldFormat);
        if (std::holds_alternative<String>(res)) {
            log_printf("Failed to load project file %s: %s\n", StringAsCStr(testOldFormat), StringAsCStr(std::get<String>(res)));
            return;
        }
        auto projectFile = std::get<std::shared_ptr<project_file>>(res);
        DAW::ProjectFileV2::saveProjectToJsonFile(projectFile, "test_v2.bsp");
        TEST_END();
    }
}

int main() {
    App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    test_projectfile_loader();
    test_projectfile_v2();
    return 0;
}
