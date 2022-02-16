#include "TestBase.hpp"
#include "common/test_common.h"
#include "fileio.h"
#include "midi/MidiFile.h"
#include "platform.h"
#include "str_util.h"
#include <vector>

namespace {

void test_midifile_loader() {
  TEST_BEGIN("test_midifile_loader");
  std::vector<FileFound> midiFilesToTest;
  log_printf("cwd %s\n",
             StringAsCStr(App::Platform::getCurrentWorkingDirectory()));
  findFilesWithExt("cpp-test-data/midifiles/", "mid", true, midiFilesToTest);
  log_printf("cpp-test-data/midifiles: %zu files\n", midiFilesToTest.size());
  int64_t numFails = 0;
  int64_t numNoTracks = 0;
  for (const FileFound &file : midiFilesToTest) {
    MidiFile midiFile;
    if (!midiFile.read(file.path)) {
      log_printf("%s failed to load\n", StringAsCStr(file.path));
      numFails++;
      continue;
    }
    int tracks = midiFile.getTrackCount();
    if (!tracks) {
      numNoTracks++;
    }
  }
  log_printf("%zd / %zu files failed to load\n", numFails,
             midiFilesToTest.size());
  log_printf("%zd / %zu files had 0 tracks\n", numNoTracks,
             midiFilesToTest.size());
  TEST_END();
}

} // namespace

int main() {
  test_midifile_loader();
  return 0;
}
