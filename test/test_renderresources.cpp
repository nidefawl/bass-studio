#include "TestBase.hpp"
#include "logging.hpp"
#include "types.hpp"
#include "str_util.hpp"
#include "platform.hpp"
#include "buildinfo.h"
#include "app/renderresources_zip.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <map>

namespace {
    void test_load_resources() {
        TEST_BEGIN("test_load_resources");
        auto resData = RenderResources::getResData();
        TEST_ASSERT_THROW(resData.size() > 0);
        // unpack inmemory using libarchive
        // read the archive
        struct archive* a = archive_read_new();
        TEST_ASSERT_THROW(!!a);
        TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_read_support_filter_all(a));
        TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_read_support_format_all(a));
        TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_read_open_memory(a, resData.data(), resData.size()));
        
        std::map<String, std::vector<uint8_t>> files;
        // iterate over all files in the archive
        for (;;) {
            // read the next archive entry
            struct archive_entry* entry;
            int r = archive_read_next_header(a, &entry);
            if (r == ARCHIVE_EOF) {
                break;
            }
            if (r != ARCHIVE_OK) {
                auto errorMsg = archive_error_string(a);
                log_lf(Log::L_ERROR, "archive_read_next_header() failed: %s\n", errorMsg);
                break;
            }
            auto pathName = archive_entry_pathname(entry);
            if (archive_entry_filetype(entry) == AE_IFREG) {
                auto size = archive_entry_size(entry);
                TEST_ASSERT_THROW(size > 0);
                TEST_ASSERT_THROW(pathName != nullptr);
                auto pathNameStr = String(pathName);
                auto buffer = std::vector<uint8_t>(size);
                auto readsize = archive_read_data(a, buffer.data(), buffer.size());
                TEST_ASSERT_THROW(readsize == ssize_t(buffer.size()));
                files[pathNameStr] = buffer;
            } else {
                log_lf(Log::L_ERROR, "file type in archive: %s\n", pathName);
            }
        }
        // iterate over map and print file name and size in kilobytes
        for (auto& [name, data] : files) {
            log_lf(Log::L_INFO, "file: %s, size: %.1f KB\n", name.c_str(), data.size() / 1024.0);
        }
        TEST_END();
    }
}// namespace

int main() {
    App::Platform::initPlatformEnvironment(BuildInfo::PRODUCT_NAME_LOWER);
    test_load_resources();
    return 0;
}
