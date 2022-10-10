#include "TestBase.hpp"
#include "common/test_common.h"
#include "logging.h"
#include "str_util.h"
#include <archive.h>
#include <archive_entry.h>
#include <cstddef>
#include <memory>

namespace {

void test_libarchive() {
    String textContentFile1 = "This is just some text content for file 1";
    std::byte binaryContentFile2_256[256];
    /* create a new gzip archive file on disk containing 3 files:
       - a file named "file1.txt" containing textContentFile1
       - a file named "file2.bin" containing binaryContentFile2_256
       - a file named "file3.bin" containing binaryContentFile2_256 */
    
    // create a new archive
    struct archive* a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, "test_libarchive.tar.gz");

    // add a file to the archive
    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, "file1.txt");
    archive_entry_set_size(entry, textContentFile1.size());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, textContentFile1.data(), textContentFile1.size());
    archive_entry_free(entry);

    // add a file to the archive
    entry = archive_entry_new();
    archive_entry_set_pathname(entry, "file2.bin");
    archive_entry_set_size(entry, sizeof(binaryContentFile2_256));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    binaryContentFile2_256[64] = std::byte(0x12);
    archive_write_data(a, binaryContentFile2_256, sizeof(binaryContentFile2_256));
    archive_entry_free(entry);

    // add a file to the archive
    entry = archive_entry_new();
    archive_entry_set_pathname(entry, "file3.bin");
    archive_entry_set_size(entry, sizeof(binaryContentFile2_256));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    binaryContentFile2_256[64] = std::byte(0x13);
    archive_write_data(a, binaryContentFile2_256, sizeof(binaryContentFile2_256));
    archive_entry_free(entry);

    // finish writing the archive
    archive_write_close(a);
    archive_write_free(a);

    // read the archive
    a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);
    archive_read_open_filename(a, "test_libarchive.tar.gz", 10240);

    // iterate over all files in the archive
    for (;;) {
        // read the next archive entry
        int r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }
        if (r != ARCHIVE_OK) {
            // log_error("archive_read_next_header() failed: %s", archive_error_string(a));
            break;
        }
        if (archive_entry_filetype(entry) == AE_IFREG) {
            auto size = archive_entry_size(entry);
            TEST_ASSERT_THROW(size > 0);
            auto pathName = archive_entry_pathname(entry);
            TEST_ASSERT_THROW(pathName != nullptr);
            auto pathNameStr = String(pathName);
            auto buffer = std::shared_ptr<std::byte[]>(new std::byte[size]);
            ssize_t readsize = archive_read_data(a, buffer.get(), size);
            if (pathNameStr == "file1.txt") {
                TEST_ASSERT_THROW(readsize == textContentFile1.size());
                String fileContent = String((char*)buffer.get(), readsize);
                TEST_ASSERT_THROW(fileContent == textContentFile1);
            } else if (pathNameStr == "file2.bin") {
                TEST_ASSERT_THROW(readsize == sizeof(binaryContentFile2_256));
                TEST_ASSERT_THROW(buffer[64] == std::byte(0x12));
            } else if (pathNameStr == "file3.bin") {
                TEST_ASSERT_THROW(readsize == sizeof(binaryContentFile2_256));
                TEST_ASSERT_THROW(buffer[64] == std::byte(0x13));
            } else {
                TEST_ASSERT_THROW(false);
            }
        } else {
            // log_error("unexpected file type in archive: %s", archive_entry_pathname(entry));
        }
    }
}       

} // namespace

int main() {
  test_libarchive();
  return 0;
}
