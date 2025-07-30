#include "TestBase.hpp"
#include "common/test_common.hpp"
#include "fileio.hpp"
#include "logging.hpp"
#include "platform.hpp"
#include "buildinfo.h"
#include <vector>

namespace {

void test_findFilesWithExt_recursive() {
  TEST_BEGIN("test_findFilesWithExt_recursive");
  std::vector<FileFound> files;
  auto resPath = TEST_PATH("findfiles");
  findFilesWithExt(resPath, "txt", true, files);
  log_lf(Log::L_INFO, "findFilesWithExt %zu\n", files.size());
  for (auto &file : files) {
    auto path = file.path;
    auto bExists = FileExists(path);
    TEST_ASSERT_EQUAL(bExists, true);
    log_lf(Log::L_INFO, "%s\n", StringAsCStr(file.path));
  }
  TEST_ASSERT_EQUAL(files.size(), 4U);
  TEST_END();
}

void test_findFilesWithExt() {
  TEST_BEGIN("test_findFilesWithExt");
  std::vector<FileFound> files;
  auto resPath = TEST_PATH("findfiles");
  findFilesWithExt(resPath, "zip", false, files);
  log_lf(Log::L_INFO, "findFilesWithExt %zu\n", files.size());
  TEST_ASSERT_EQUAL(files.size(), 0U);
  findFilesWithExt(resPath, "zip", true, files);
  TEST_ASSERT_EQUAL(files.size(), 2U);
  TEST_END();
}

void test_findDirectoriesWithExt() {
  TEST_BEGIN("test_findDirectoriesWithExt");
  std::vector<FileFound> files;
  auto resPath = TEST_PATH("findfiles");
  findDirectoriesWithExt(resPath, "test", files);
  log_lf(Log::L_INFO, "findDirectoriesWithExt %zu\n", files.size());
  TEST_ASSERT_EQUAL(files.size(), 1U);
  for (auto &file : files) {
    log_lf(Log::L_INFO, "%s\n", StringAsCStr(file.path));
    TEST_ASSERT_EQUAL(PathIsDirectory(file.path), true);
  }
  TEST_END();
}

void test_listFilesystemNonRecursive() {
  TEST_BEGIN("test_listFilesystemNonRecursive");
  std::vector<FileFound> files;
  std::vector<String> fileExtensions;
  const auto resPath = TEST_PATH("filebrowser");
  listFilesystemNonRecursive(resPath, fileExtensions, files);
  TEST_ASSERT_EQUAL(files.size(), 3U);
  for (auto& file : files) {
    TEST_ASSERT_EQUAL(file.bIsDir, true);
    std::vector<FileFound> filesSub;
    listFilesystemNonRecursive(file.path, fileExtensions, filesSub);
    if ("1file" == file.name) {
      TEST_ASSERT_EQUAL(filesSub.size(), 1U);
      for (auto& fileSub : filesSub) {
        TEST_ASSERT_EQUAL(fileSub.bIsDir, false);
      }
    } else if ("2files" == file.name) {
      TEST_ASSERT_EQUAL(filesSub.size(), 2U);
      for (auto& fileSub : filesSub) {
        TEST_ASSERT_EQUAL(fileSub.bIsDir, false);
      }
    } else if ("3files-nested" == file.name) {
      TEST_ASSERT_EQUAL(filesSub.size(), 3U);
      for (auto& fileSub : files) {
        TEST_ASSERT_EQUAL(fileSub.bIsDir, true);
      }
    }
    log_lf(Log::L_INFO, "%s OK\n", StringAsCStr(file.path));
  }
  fileExtensions.emplace_back("preset");
  files.clear();
  listFilesystemNonRecursive(resPath, fileExtensions, files);
  TEST_ASSERT_EQUAL(files.size(), 3U);
  for (auto& file : files) {
    TEST_ASSERT_EQUAL(file.bIsDir, true);
    std::vector<FileFound> filesSub;
    listFilesystemNonRecursive(file.path, fileExtensions, filesSub);
    if ("0files" == file.name) {
      TEST_ASSERT_EQUAL(filesSub.size(), 0U);
    } else if ("1file" == file.name) {
      TEST_ASSERT_EQUAL(filesSub.size(), 0U);
      for (auto& fileSub : filesSub) {
        TEST_ASSERT_EQUAL(fileSub.bIsDir, false);
      }
    } else if ("2files" == file.name) {
      TEST_ASSERT_EQUAL(filesSub.size(), 0U);
      for (auto& fileSub : filesSub) {
        TEST_ASSERT_EQUAL(fileSub.bIsDir, false);
      }
    } else if ("3files-nested" == file.name) {
      TEST_ASSERT_EQUAL(filesSub.size(), 3U);
      std::vector<FileFound> filesSub2;
      for (auto& fileSub : filesSub) {
        TEST_ASSERT_EQUAL(fileSub.bIsDir, true);
        listFilesystemNonRecursive(fileSub.path, fileExtensions, filesSub2);
      }
      TEST_ASSERT_EQUAL(filesSub2.size(), 2U);
      for (auto& fileSub : filesSub2) {
        TEST_ASSERT_EQUAL(fileSub.bIsDir, false);
      }
    }
    log_lf(Log::L_INFO, "%s OK\n", StringAsCStr(file.path));
  }
  TEST_END();
}

} // namespace

int main() {
  App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
  test_findDirectoriesWithExt();
  test_findFilesWithExt_recursive();
  test_findFilesWithExt();
  test_listFilesystemNonRecursive();
  return 0;
}
