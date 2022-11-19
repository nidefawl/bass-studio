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
String pathResources;// read only app resource directory: C:/program files/daw
String pathUserdata; // writable app directory: C:/users/user/appdata/daw/
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

String GetResourcePath() {
    return App::Platform::pathResources;
}

String GetUserdataPath() {
    return App::Platform::pathUserdata;
}

void setResourcePath(String cwd) {
    sanitizePathToDirectory(cwd);
    App::Platform::pathResources = cwd;
}

void setUserdataPath(String cwd) {
    sanitizePathToDirectory(cwd);
    pathUserdata = cwd;
}

void initPlatformEnvironment(const String& appname, const String& optionalCwd) {
    String cwdPath = !optionalCwd.empty() ? optionalCwd : getCurrentWorkingDirectory();
#ifdef __APPLE__
    String resourcePath = cwdPath + "/res";
    if (cwdPath.empty() || cwdPath == "/") {
        cwdPath = GetExecutablePath();
        auto p = cwdPath.find_last_of('/');
        if (p != String::npos) {
            cwdPath = cwdPath.substr(0, p);
        }
        resourcePath = cwdPath + "/../Resources/res";
    }
    if (!FileExists(resourcePath)) {
        log_lf(Log::L_DEBUG, "resource path not found: %s", resourcePath.c_str());
        resourcePath = cwdPath + "/res";
    }
    if (!FileExists(resourcePath)) {
        log_lf(Log::L_DEBUG, "resource path not found: %s", resourcePath.c_str());
        resourcePath = cwdPath + "/../Resources/res";
    }
    if (!FileExists(resourcePath)) {
        log_lf(Log::L_DEBUG, "resource path not found: %s", resourcePath.c_str());
        resourcePath = cwdPath + "/../res";
    }
    log_lf(Log::L_DEBUG, "Keeping resource path: %s", resourcePath.c_str());

#else
    String resourcePath = cwdPath + FILE_PATHSEP_STR + "res";
    if (!FileExists(resourcePath)) {
        resourcePath = cwdPath + FILE_PATHSEP_STR + ".." + FILE_PATHSEP_STR + "res";
    }
#endif
    setResourcePath(resourcePath);

    String userDataPath = appname;
    if(determineUserdataPath(userDataPath)) {
        userDataPath = userDataPath + FILE_PATHSEP_STR + appname;
    }
    setUserdataPath(userDataPath);
    if (!App::Platform::pathUserdata.empty()) {
        CreateDirectoryIfNotExists(App::Platform::pathUserdata);
    }
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
