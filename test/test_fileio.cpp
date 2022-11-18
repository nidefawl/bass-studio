#include "TestBase.hpp"
#include "common/test_common.h"
#include "fileio.h"
#include "platform.h"
#include "buildinfo.h"
#include <vector>

namespace {

void test_findFilesWithExt_recursive() {
  TEST_BEGIN("test_findFilesWithExt_recursive");
  std::vector<FileFound> files;
  auto resPath = App::Platform::toResourcePath("fonts");
  findFilesWithExt(resPath, "ttf", true, files);
  printf("findFilesWithExt %zu\n", files.size());
  for (auto &file : files) {
    printf("%s\n", StringAsCStr(file.path));
  }
  TEST_END();
}

void test_findFilesWithExt_non_recursive() {
  TEST_BEGIN("test_findFilesWithExt_non_recursive");
  std::vector<FileFound> files;
  auto resPath = App::Platform::toResourcePath("fonts");
  findFilesWithExt(resPath, "ttf", false, files);
  printf("findFilesWithExt %zu\n", files.size());
  for (auto &file : files) {
    printf("%s\n", StringAsCStr(file.path));
  }
  TEST_END();
}

} // namespace

int main() {
  App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
  test_findFilesWithExt_recursive();
  test_findFilesWithExt_non_recursive();
  return 0;
}
