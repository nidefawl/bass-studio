#include "TestBase.hpp"
#include "common/test_common.h"
#include "fileio.h"
#include "logging.h"
#include "platform.h"
#include "buildinfo.h"
#include <vector>

namespace {

void test_findFilesWithExt_recursive() {
  TEST_BEGIN("test_findFilesWithExt_recursive");
  std::vector<FileFound> files;
  auto resPath = TEST_PATH("findfiles");
  findFilesWithExt(resPath, "txt", true, files);
  log_lf(Log::L_INFO, "findFilesWithExt %zu\n", files.size());
  TEST_ASSERT_EQUAL(files.size(), 4U);
  for (auto &file : files) {
    log_lf(Log::L_INFO, "%s\n", StringAsCStr(file.path));
  }
  TEST_END();
}

void test_findFilesWithExt_non_recursive() {
  TEST_BEGIN("test_findFilesWithExt_non_recursive");
  std::vector<FileFound> files;
  auto resPath = TEST_PATH("findfiles");
  findFilesWithExt(resPath, "zips", false, files);
  log_lf(Log::L_INFO, "findFilesWithExt %zu\n", files.size());
  TEST_ASSERT_EQUAL(files.size(), 0U);
  TEST_END();
}

void test_listFilesystemNonRecursive() {
  TEST_BEGIN("test_listFilesystemNonRecursive");
  std::vector<FileFound> files;
  std::vector<String> fileExtensions;
  auto resPath = TEST_PATH("filebrowser");
  listFilesystemNonRecursive(resPath, fileExtensions, files);
  TEST_ASSERT_EQUAL(files.size(), 3U);
  for (auto& file : files) {
    log_lf(Log::L_INFO, "%s\n", StringAsCStr(file.path));
    TEST_ASSERT_EQUAL(file.bIsDir, true);
  }
  TEST_END();
}

} // namespace

int main() {
  App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
  test_findFilesWithExt_recursive();
  test_findFilesWithExt_non_recursive();
  test_listFilesystemNonRecursive();
  return 0;
}
