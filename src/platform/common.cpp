#include "archive.h"
#include "archive_entry.h"
#include "fileio.h"
#include "exceptions.h"
#include <cstdlib>
#include <cstdio>
#include "logging.h"
#include "platform.h"
#include "str_util.h"
#include <stb/stb_image.h>

namespace App::Platform {

namespace {
String pathResources;               // read only app resource directory: C:/program files/daw/res/
String pathUserdata;                // writable app directory: C:/users/user/appdata/daw/
String pathDefaultSettingFiles;     // read only app default setting files directory: C:/program files/daw/defaults/
}

String toResourcePath(const String& relPath) {
    String path = App::Platform::pathResources + relPath;
    sanitizePathToFile(path);
    return path;
}

String toUserdataPath(const String& relPath) {
    String path = App::Platform::pathUserdata + relPath;
    sanitizePathToFile(path);
    return path;
}

String toDefaultSettingFilesPath(const String& relPath) {
    String path = App::Platform::pathDefaultSettingFiles + relPath;
    sanitizePathToFile(path);
    return path;
}

String GetResourcePath() {
    return App::Platform::pathResources;
}

String GetUserdataPath() {
    return App::Platform::pathUserdata;
}

String GetDefaultSettingFilesPath() {
    return App::Platform::pathDefaultSettingFiles;
}

void setResourcePath(String cwd) {
    sanitizePathToDirectory(cwd);
    App::Platform::pathResources = cwd;
}

void setUserdataPath(String cwd) {
    sanitizePathToDirectory(cwd);
    App::Platform::pathUserdata = cwd;
}

void setDefaultSettingFilesPath(String cwd) {
    sanitizePathToDirectory(cwd);
    App::Platform::pathDefaultSettingFiles = cwd;
}

void extractDefaultPresets() {
	// TEST_ASSERT_EQUAL(ARCHIVE_OK, archive_read_open_filename(a, outName.c_str(), 10240));
    auto defaultsArchive = toDefaultSettingFilesPath("data.zip");
    if (!FileExists(defaultsArchive)) {
        log_lf(Log::L_ERROR, "data.zip not found in %s\n", StringAsCStr(defaultsArchive));
        return;
    }
    auto a = archive_read_new();
	if (!a || 
        archive_read_support_filter_all(a) != ARCHIVE_OK ||
        archive_read_support_format_all(a) != ARCHIVE_OK) {
        log_lf(Log::L_ERROR, "archive_read_new() failed\n");
        return;
    }
    if (archive_read_open_filename(a, StringAsCStr(defaultsArchive), 10240) != ARCHIVE_OK) {
        auto errorMsg = archive_error_string(a);
        log_lf(Log::L_ERROR, "archive_read_open_filename() failed: %s\n", errorMsg);
        archive_read_free(a);
        return;
    }
    // iterate over all files in the archive
    for (;;) {
        // read the next archive entry
        struct archive_entry* entry = nullptr;
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
            String strPathUserdata = toUserdataPath(pathName);
            if (!FileExists(strPathUserdata)) {
                log_lf(Log::L_INFO, "extracting: %s to %s\n", pathName, StringAsCStr(strPathUserdata));
                auto size = archive_entry_size(entry);
                std::vector<uint8_t> buffer(size);
                auto readsize = archive_read_data(a, buffer.data(), size);
                if (readsize < ARCHIVE_OK) {
                    auto errorMsg = archive_error_string(a);
                    log_lf(Log::L_ERROR, "archive_read_data() failed: %s\n", errorMsg);
                    break;
                }
                String path;
                SplitPath(strPathUserdata, &path, nullptr, nullptr, nullptr);
                CreateDirectoryIfNotExists(path);
                WriteFileVector(strPathUserdata, buffer);
            }
        }
    }
    if (archive_read_free(a) != ARCHIVE_OK) {
        auto errorMsg = archive_error_string(a);
        log_lf(Log::L_ERROR, "archive_read_free() failed: %s\n", errorMsg);
    }
}

void initPlatformEnvironment(const String& appname, const String& optionalCwd) {
    String cwdPath = !optionalCwd.empty() ? optionalCwd : getCurrentWorkingDirectory();
#ifdef __APPLE__
    String resourcePath = cwdPath + "/res";
    String defaultsPath = cwdPath + "/defaults";
    if (cwdPath.empty() || cwdPath == "/") {
        cwdPath = GetExecutablePath();
        auto p = cwdPath.find_last_of('/');
        if (p != String::npos) {
            cwdPath = cwdPath.substr(0, p);
        }
        resourcePath = cwdPath + "/../Resources/res";
        defaultsPath = cwdPath + "/../Resources/defaults";
    }
    if (!FileExists(resourcePath)) {
        resourcePath = cwdPath + "/res";
    }
    if (!FileExists(resourcePath)) {
        resourcePath = cwdPath + "/../Resources/res";
    }
    if (!FileExists(resourcePath)) {
        resourcePath = cwdPath + "/../res";
    }
    if (!FileExists(defaultsPath)) {
        defaultsPath = cwdPath + "/defaults";
    }
    if (!FileExists(defaultsPath)) {
        defaultsPath = cwdPath + "/../Resources/defaults";
    }
    if (!FileExists(defaultsPath)) {
        defaultsPath = cwdPath + "/../defaults";
    }

#else
    String resourcePath = cwdPath + FILE_PATHSEP_STR + "res";
    if (!FileExists(resourcePath)) {
        resourcePath = cwdPath + FILE_PATHSEP_STR + ".." + FILE_PATHSEP_STR + "res";
    }
    String defaultsPath = cwdPath + FILE_PATHSEP_STR + "defaults";
    if (!FileExists(defaultsPath)) {
        defaultsPath = cwdPath + FILE_PATHSEP_STR + ".." + FILE_PATHSEP_STR + "defaults";
    }
    if (!FileExists(defaultsPath)) {
        defaultsPath = cwdPath + FILE_PATHSEP_STR + ".." + FILE_PATHSEP_STR + "dist" + FILE_PATHSEP_STR + "defaults";
    }
#endif
    setResourcePath(resourcePath);
    setDefaultSettingFilesPath(defaultsPath);

    String userDataPath = appname;
    if(determineUserdataPath(userDataPath)) {
#if defined(__linux__) || defined(__APPLE__)
        userDataPath = userDataPath + FILE_PATHSEP_STR + "." + appname;
#else
        userDataPath = userDataPath + FILE_PATHSEP_STR + appname;
#endif
    }
    setUserdataPath(userDataPath);
    if (!App::Platform::pathUserdata.empty()) {
        CreateDirectoryIfNotExists(App::Platform::pathUserdata);
    }
    extractDefaultPresets();
}

int32_t createUniqueFilename(String& pathString, const String& baseName) {
    String name;
    String ext;
    String path;
    int32_t idx = 0;
    pathString = baseName;
    SplitPath(baseName, &path, &name, &ext);
    App::Platform::sanitizePathToDirectory(path);
    while (FileExists(pathString)&&++idx<10000) {
        String nextPath = path;
        nextPath += name;
        nextPath += "-";
        nextPath += std::to_string(idx);
        nextPath += ".";
        nextPath += ext;
        idx++;
        pathString = nextPath;
    }
    return idx;
}

} // namespace App::Platform

#define  READALL_OK           0   /* Success */
#define  READALL_INVALID    (-1)  /* Invalid parameters */
#define  READALL_ERROR      (-2)  /* Stream error */
#define  READALL_TOOMUCH    (-3)  /* Too much input */
#define  READALL_NOMEM      (-4)  /* Out of memory */
/* Size of each input chunk to be
   read and allocate for. */
#ifndef  READALL_CHUNK
#define  READALL_CHUNK  (1<<21) /* 2MB */
#endif


/* This function returns one of the READALL_ constants above.
   If the return value is zero == READALL_OK, then:
     (*dataptr) points to a dynamically allocated buffer, with
     (*sizeptr) chars read from the file.
     The buffer is allocated for one extra char, which is NUL,
     and automatically appended after the data.
   Initial values of (*dataptr) and (*sizeptr) are ignored.
*/
int readall(FILE* in, char** dataptr, size_t* sizeptr) {
    char *data  = nullptr, *temp;
    size_t size = 0;
    size_t used = 0;
    size_t n;

    /* None of the parameters can be nullptr. */
    if (in == nullptr || dataptr == nullptr || sizeptr == nullptr)
        return READALL_INVALID;

    /* A read error already occurred? */
    if (ferror(in))
        return READALL_ERROR;

    while (1) {

        if (used + READALL_CHUNK + 1 > size) {
            size = used + READALL_CHUNK + 1;

            /* Overflow check. Some ANSI C compilers
               may optimize this away, though. */
            if (size <= used) {
                free(data);
                return READALL_TOOMUCH;
            }

            temp = (char*) realloc(data, size);
            if (temp == nullptr) {
                free(data);
                return READALL_NOMEM;
            }
            data = temp;
        }

        n = fread(data + used, 1, READALL_CHUNK, in);
        if (n == 0)
            break;

        used += n;
    }

    if (ferror(in)) {
        free(data);
        return READALL_ERROR;
    }

    temp = (char*) realloc(data, used + 1);
    if (temp == nullptr) {
        free(data);
        return READALL_NOMEM;
    }
    data       = temp;
    data[used] = '\0';

    *dataptr = data;
    *sizeptr = used;

    return READALL_OK;
}

int64_t ReadFileText(const String& filename, String& out, int resourceType) {
    String fileResPath;
    if (resourceType == 0) {
        fileResPath = App::Platform::toResourcePath(filename);
    } else {
        fileResPath = App::Platform::toUserdataPath(filename);
    }

    const char* fname = StringAsCStr(fileResPath);

    FILE* fp = fopen(fname, "r");
    if (!fp) {
        int err = errno;
        log_lf(Log::L_WARN, "Failed opening file %s: %s (%d)\n", fname, strerror(err), err);
    }
    if (fp != nullptr) {
        char* buf;
        size_t len;
        int ret = readall(fp, &buf, &len);
        fclose(fp);
        if (ret == READALL_OK) {
            if (buf) {
                out = buf;
                free(buf);
            }
            return (int64_t) len;
        }
    }
    return -1;
}


int64_t ReadImage(const String& Filename, ImageBuf& ref) {
    String path = App::Platform::toResourcePath(Filename);
    if (!FileExists(path)) {
        throw FileIOException(StringFormat("File not found: %s", StringAsCStr(path)));
    }
    unsigned char* data = stbi_load(StringAsCStr(path), &ref.w, &ref.h, &ref.bitdepth, 0);
    if (!data) {
        throw FileIOException(StringFormat("%s: %s", StringAsCStr(path), stbi_failure_reason()));
    }
    int64_t bufSize = ref.w * ref.h * ref.bitdepth;
    ref.bytes.reserve(bufSize);
    ref.bytes.assign(data, data + bufSize);
    stbi_image_free(data);
    return bufSize;
}
