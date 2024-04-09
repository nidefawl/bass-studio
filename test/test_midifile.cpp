#include "TestBase.hpp"
#include "common/test_common.h"
#include "fileio.h"
#include "fileloader.h"
#include "logging.h"
#include "midi/MidiFile.h"
#include "platform.h"
#include "str_util.h"
#include <vector>

namespace {

    void test_midifile_loader() {
        TEST_BEGIN("test_midifile_loader");
        std::vector<FileFound> midiFilesToTest;
        findFilesWithExt(TEST_PATH("midifiles"), "mid", true, midiFilesToTest);
        log_printf(TEST_PATH("midifiles: %zu files\n"), midiFilesToTest.size());
        int64_t numFails    = 0;
        int64_t numNoTracks = 0;
        for (const FileFound& file : midiFilesToTest) {
            LoadMidiTask task(file.path);
            try {
                task.run();
                auto clipboard = task.getClipboard();
                if (!clipboard || clipboard->tracks.empty()) {
                    log_printf("no tracks in clipboard\n");
                    numNoTracks++;
                } else {
                    log_printf("clipboard has %zu tracks\n", clipboard->tracks.size());
                }
            } catch (std::exception& e) {
                log_lf(Log::L_ERROR, "Failed loading %s\n", StringAsCStr(file.path));
                numFails++;
            }
        }
        log_printf("%zd / %zu files failed to load\n", numFails, midiFilesToTest.size());
        log_printf("%zd / %zu files had 0 tracks\n", numNoTracks, midiFilesToTest.size());
        TEST_END();
    }

}// namespace

int main() {
    test_midifile_loader();
    return 0;
}
