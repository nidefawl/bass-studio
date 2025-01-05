#include "TestBase.hpp"
#include "common/test_common.h"
#include "logging.h"
#include "str_util.h"
#include <archive.h>
#include <archive_entry.h>
#include <cstddef>
#include <memory>

namespace {

void test_libarchive(String outName = "test_libarchive.zip") {
    TEST_BEGIN("write zip file");
    String textContentFile1 = "This is just some text content for file 1";
    std::vector<std::byte> binaryContentFile2_256(1024LL*1024*32);
    for (size_t i = 0; i < binaryContentFile2_256.size(); ++i) {
        binaryContentFile2_256[i] = std::byte(((i+i%10) % 256));
    }
    /* create a new gzip archive file on disk containing 3 files:
       - a file named "file1.txt" containing textContentFile1
       - a file named "file2.bin" containing binaryContentFile2_256
       - a file named "file3.bin" containing binaryContentFile2_256 */
    
    // create a new archive
    struct archive* a = archive_write_new();
	TEST_ASSERT_THROW(archive_write_set_format_zip(a) >= ARCHIVE_OK);
	TEST_ASSERT_THROW(archive_write_zip_set_compression_deflate(a) >= ARCHIVE_WARN);
	// /* Disable Zip64 extensions. */
	// TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_write_set_format_option(a, "zip", "zip64", NULL));
	TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_write_open_filename(a, outName.c_str()));

    // add a file to the archive
    struct archive_entry* entry = archive_entry_new();
	TEST_ASSERT_THROW(!!entry);
    archive_entry_set_pathname(entry, "file1.txt");
    archive_entry_set_size(entry, ssize_t(textContentFile1.size()));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
	TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_write_header(a, entry));
	TEST_ASSERT_EQUAL(la_ssize_t(textContentFile1.size()), archive_write_data(a, textContentFile1.data(), textContentFile1.size()));
    archive_entry_free(entry);

    // add a file to the archive
    entry = archive_entry_new();
	TEST_ASSERT_THROW(!!entry);
    archive_entry_set_pathname(entry, "file2.bin");
    archive_entry_set_size(entry, binaryContentFile2_256.size());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
	TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_write_header(a, entry));
    binaryContentFile2_256[64] = std::byte(0x12);
    TEST_ASSERT_EQUAL(la_ssize_t(binaryContentFile2_256.size()), archive_write_data(a, binaryContentFile2_256.data(), binaryContentFile2_256.size()));
    archive_entry_free(entry);

    // add a file to the archive
    entry = archive_entry_new();
    archive_entry_set_pathname(entry, "file3.bin");
    archive_entry_set_size(entry, binaryContentFile2_256.size());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
	TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_write_header(a, entry));
    binaryContentFile2_256[64] = std::byte(0x13);
    TEST_ASSERT_EQUAL(la_ssize_t(binaryContentFile2_256.size()), archive_write_data(a, binaryContentFile2_256.data(), binaryContentFile2_256.size()));
    archive_entry_free(entry);

    // finish writing the archive
    TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_write_close(a));
    TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_write_free(a));

    // read the archive
    a = archive_read_new();
	TEST_ASSERT_THROW(!!a);
	TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_read_support_filter_all(a));
	TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_read_support_format_all(a));
	TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_read_open_filename(a, outName.c_str(), 10240));
    // iterate over all files in the archive
    for (;;) {
        // read the next archive entry
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
            auto buffer = std::shared_ptr<std::byte[]>(new std::byte[size]);
            auto readsize = archive_read_data(a, buffer.get(), size);
            if (pathNameStr == "file1.txt") {
                TEST_ASSERT_THROW(readsize == ssize_t(textContentFile1.size()));
                String fileContent = String((char*)buffer.get(), readsize);
                TEST_ASSERT_THROW(fileContent == textContentFile1);
            } else if (pathNameStr == "file2.bin") {
                TEST_ASSERT_THROW(readsize == ssize_t(binaryContentFile2_256.size()));
                TEST_ASSERT_THROW(buffer[64] == std::byte(0x12));
            } else if (pathNameStr == "file3.bin") {
                TEST_ASSERT_THROW(readsize == ssize_t(binaryContentFile2_256.size()));
                TEST_ASSERT_THROW(buffer[64] == std::byte(0x13));
            } else {
                TEST_ASSERT_THROW(false);
            }
        } else {
            log_lf(Log::L_ERROR, "file type in archive: %s\n", pathName);
        }
    }
    TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_read_close(a));
    TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_read_free(a));
    TEST_END();
}

} // namespace

int main() {
  test_libarchive();
//   test_libarchive("");
  return 0;
}
