#include "TestBase.hpp"
#include "common/test_common.h"
#include "fileio.h"
#include "logging.h"
#include "platform.h"
#include "projectfile.h"
#include "str_util.h"
#include <vector>
#include <memory>

namespace {
    void test_projectfile_loader() {
        TEST_BEGIN("test_projectfile_loader");
        std::vector<FileFound> midiFilesToTest;
        log_printf("cwd %s\n",
                   StringAsCStr(App::Platform::getCurrentWorkingDirectory()));
        findFilesWithExt("cpp-test-data/projects", "project", true, midiFilesToTest);
        log_printf("projects: %zu files\n", midiFilesToTest.size());
        int64_t numSuccess  = 0;
        int64_t numFails    = 0;
        int64_t numNoTracks = 0;
        int64_t numLoaded   = 0;

        for (const FileFound& file : midiFilesToTest) {
            numLoaded++;
            std::shared_ptr<project_file> projectFile;
            String path = file.path;
            projectFile = loadProjectFromJsonFile(path);
            if (!projectFile) {
                numFails++;
                log_lf(Log::L_ERROR, "Error: failed loading file %s\n", StringAsCStr(path));
            } else {
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
        }
        log_lf(Log::L_DEBUG, "%zd / %zu files failed to load\n", numFails, numLoaded);
        log_lf(Log::L_DEBUG, "%zd / %zu files had 0 tracks\n", numNoTracks, numSuccess);
        TEST_END();
    }
}// namespace

int main() {
    App::Platform::initPlatformEnvironment("daw");
    test_projectfile_loader();
    return 0;
}
