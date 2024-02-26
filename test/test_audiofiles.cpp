#include "TestBase.hpp"
#include "fileio.h"
#include "host/audiocache/audiocache.h"
#include "logging.h"
#include "tls.h"
#include <vector>

namespace {

    void test_audiofile_loading() {
        TEST_BEGIN("test_midifile_loader");
        auto fileTypes = std::array{ SUPPORTED_AUDIO_FILE_TYPES };
        std::vector<FileFound> files;
        for (const auto& fileType : fileTypes) {
            findFilesWithExt(TEST_PATH("samples"), fileType, false, files);
        }
        log_printf(TEST_PATH("audiofiles: %zu files\n"), files.size());
        audiocache cache(44100);
        for (const FileFound& file : files) {
            audiofile_t* audiofile = cache.loadFile(file.path, -1, "", nullptr, nullptr);
            if (audiofile) {
                log_lf(Log::L_INFO, "loaded audiofile: %s - %u channels, %zd samples\n", audiofile->name.c_str(), audiofile->sample->nChannels, audiofile->sample->nSamples);
                cache.unloadSampleId(audiofile->id);
            } else {
                log_lf(Log::L_ERROR, "Failed to load audiofile: %s\n", file.path.c_str());
            }
        }
        TEST_END();
    }

}// namespace

int main() {
    setExceptionHandler();
    daw_tls::initNewTls();
    test_audiofile_loading();
    return 0;
}
