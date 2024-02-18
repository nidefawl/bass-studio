#if defined(__linux__) || defined(__APPLE__)
#include "fileio.h"
#include "exceptions.h"
#include "str_util.h"
#include "assert_dbg.h"
#include "window.h"
#include "platform.h"
#include "logging.h"

#include <stb/stb_image.h>
#include <vector>
#include <iostream>
#include <string>

#include <limits>
#include <cstdio>
#include "types.h"
#include <cstdlib>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <fts.h>

using std::size_t;
using std::vector;

void ThrowLastErrorIf(bool expression, const String& msg) {
    if (expression) {
        throw FileIOException(errno, msg);
    }
}

class File {
    int handle = -1;

public:
    explicit File(const String& filename, int mode, int perms) {
        handle = open(StringAsCStr(filename), mode, perms);
        ThrowLastErrorIf(handle < 0, "open call failed on file named " + filename);
    }

    ~File() {
        if (handle > -1) {
            always_assert(0 == close(handle));
        }
    }

    /* Disable copies */
    File& operator=(const File&) = delete;
    File(const File&) = delete;
    File& operator=( File&&) = delete;
    File(File&&) = delete;

    int GetHandle() { return handle; }
};

size_t GetFileSizeSafe(const String& filename) {
    struct stat fStat{};
    if (stat(StringAsCStr(filename), &fStat) == 0) {
        return fStat.st_size;
    }
    return 0;
}

int32_t WriteFileVector(const String& filename, const std::vector<uint8_t>& writebuffer) {
    File fobj(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    const auto bufferSize = static_cast<ssize_t>(writebuffer.size());
    ssize_t written = 0;
    while (written < bufferSize) {
        ssize_t len = write(fobj.GetHandle(), writebuffer.data(), bufferSize);
        if (len < 0) {
            int err = errno;
            if (err == EAGAIN) {
                continue;
            }
            if (err == EINTR) {
                continue;
            }
            throw FileIOException(errno, "WriteFile failed: " + filename);
        }
        written += len;
    }
    return static_cast<int32_t>(written);
}

void ReadFileVector(const String& filename, std::vector<uint8_t>& out) {
    File fobj(filename, O_RDONLY, 0);
    auto filesize   = static_cast<ssize_t>(GetFileSizeSafe(filename));
    ssize_t bytesRead = 0;

    out.resize(filesize);

    while (bytesRead < filesize) {
        ssize_t len = read(fobj.GetHandle(), out.data(), filesize);
        if (len < 0) {
            int err = errno;
            if (err == EAGAIN) {
                continue;
            }
            if (err == EINTR) {
                continue;
            }
            throw FileIOException(errno, "ReadFile failed: " + filename);
        }
        bytesRead += len;
    }
}

void findFilesWithExt(
        String strPath,
        String strExt,
        bool bRecursive,
        std::vector<FileFound>& _out) {
    FTS* file_system = nullptr;
    const char* ptr  = StringAsCStr(strPath);
    char* args[2]    = { (char*) ptr, nullptr };
    
    file_system = fts_open(args, FTS_LOGICAL | FTS_COMFOLLOW | FTS_NOCHDIR, nullptr);
    if (!file_system) {
        throw FileIOException(errno, "fts_open failed: " + strPath);
    }
    errno = 0;
    while (true) {
        FTSENT* fs_entry = fts_read(file_system);
        if (!fs_entry && errno) // ignore error and continue
            continue;
        if (!fs_entry)
            break;
        if (!(fs_entry->fts_info & FTS_D))
            continue;
        FTSENT* child = fts_children(file_system, 0);
        while (child) {
            if (!(child->fts_info & FTS_DP)) {
            String fileName, ext;
            SplitPath(child->fts_name, nullptr, nullptr, &ext, &fileName);
            if (ext == strExt) {
                String path = String(child->fts_path) + child->fts_name;
                const FileFound f = { std::move(path), child->fts_name, ext };
                _out.push_back(f);
            }
            }
            child = child->fts_link;
        }
        if (!bRecursive)
            break;
    }

    fts_close(file_system);
}


class FileTimeGetter::Impl {
    struct stat fStat{};
    bool ok = false;

public:
    int64_t getWriteTimeI64() {
        if (!ok) {
            return 0;
        }
#ifdef __APPLE__
        return fStat.st_mtimespec.tv_sec * 1000L + fStat.st_mtimespec.tv_nsec / 1000000L;
#else
        return fStat.st_mtim.tv_sec * 1000L + fStat.st_mtim.tv_nsec / 1000000L;
#endif
    }
    explicit Impl(String path) {
        ok = stat(StringAsCStr(path), &fStat) == 0;
    }
    ~Impl() = default;
};
FileTimeGetter::FileTimeGetter(const String& path) : m_impl{ new FileTimeGetter::Impl{ path } } {
}
FileTimeGetter::~FileTimeGetter() {
    delete m_impl;
}
int64_t FileTimeGetter::getWriteTimeI64() {
    return m_impl->getWriteTimeI64();
}
class FileImpl {
    FILE* m_handle;

public:
    explicit FileImpl(const String& filename, OpenFileMode mode) {
        String strFileOpenMode;
        switch (mode) {
            case OpenFileMode::READ:
                strFileOpenMode = "rb";
                break;
            case OpenFileMode::WRITE:
                strFileOpenMode = "wb";
                break;
            case OpenFileMode::READWRITE:
                strFileOpenMode = "wb";
                break;
        }
#if !defined(__APPLE__)
        m_handle = fopen64(filename.c_str(), strFileOpenMode.c_str());
#else
        m_handle = fopen(filename.c_str(), strFileOpenMode.c_str());
#endif
        ThrowLastErrorIf(m_handle == nullptr,
                         "fopen64 call failed on file named " + filename);
    }

    ~FileImpl() { (void)fclose(m_handle); }


    /* Disable copies */
    FileImpl& operator=(const FileImpl&) = delete;
    FileImpl(const FileImpl&) = delete;
    FileImpl& operator=( FileImpl&&) = delete;
    FileImpl(FileImpl&&) = delete;

    FILE* GetHandle() { return m_handle; }
};
IOFile::IOFile(FileImpl* _impl) noexcept : impl(_impl) {
    this->validHandle = true;
}
IOFile::~IOFile() {
    delete impl;
}
void IOFile::write(const char* data, size_t len) {
    if (this->validHandle) {
        (void)fwrite(data, len, 1, impl->GetHandle());
    }
}
void IOFile::flush() {
    if (this->validHandle) {
        (void)fflush(impl->GetHandle());
    }
}

IOFile* IOFile::openFile(const String& filename, OpenFileMode mode) {
    auto* impl = new FileImpl(filename, mode);
    if (!impl->GetHandle()) {
        delete impl;
        return nullptr;
    }
    return new IOFile(impl);
}

int IsDirectory(const char *path) {
   struct stat statbuf{};
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

bool CreateDirectoryIfNotExists(const String& DirPath) {
    String partPath = "";
    do {
        auto pos = DirPath.find_first_of('/', partPath.size());
        if (pos == String::npos) {
            pos = DirPath.size();
        }
        partPath = DirPath.substr(0, pos + 1);
        if (partPath.empty()) {
            break;
        }
        if (access(partPath.c_str(), F_OK) != 0) {
            if (mkdir(partPath.c_str(), 0777) != 0) {
                return false;
            }
        }
    } while (partPath.size() < DirPath.size());
    return true;
}

bool DeleteDirectory(const String& DirPath, bool bRecursive) {
    if (!assert_expr(!DirPath.empty())) {
        return false;
    }

    const char* dir = DirPath.c_str();

    int ret = 0;
    FTS *ftsp = NULL;
    FTSENT *curr;

    // Cast needed (in C) because fts_open() takes a "char * const *", instead
    // of a "const char * const *", which is only allowed in C++. fts_open()
    // does not modify the argument.
    char *files[] = { (char *) dir, NULL };

    // FTS_NOCHDIR  - Avoid changing cwd, which could cause unexpected behavior
    //                in multithreaded programs
    // FTS_PHYSICAL - Don't follow symlinks. Prevents deletion of files outside
    //                of the specified directory
    // FTS_XDEV     - Don't cross filesystem boundaries
    ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
    if (!ftsp) {
        fprintf(stderr, "%s: fts_open failed: %s\n", dir, strerror(errno));
        ret = -1;
        goto finish;
    }

    while ((curr = fts_read(ftsp))) {
        switch (curr->fts_info) {
        case FTS_NS:
        case FTS_DNR:
        case FTS_ERR:
            fprintf(stderr, "%s: fts_read error: %s\n",
                    curr->fts_accpath, strerror(curr->fts_errno));
            break;

        case FTS_DC:
        case FTS_DOT:
        case FTS_NSOK:
            // Not reached unless FTS_LOGICAL, FTS_SEEDOT, or FTS_NOSTAT were
            // passed to fts_open()
            break;

        case FTS_D:
            // Do nothing. Need depth-first search, so directories are deleted
            // in FTS_DP
            break;

        case FTS_DP:
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            if (remove(curr->fts_accpath) < 0) {
                fprintf(stderr, "%s: Failed to remove: %s\n",
                        curr->fts_path, strerror(curr->fts_errno));
                ret = -1;
            }
            break;
        }
    }

finish:
    if (ftsp) {
        fts_close(ftsp);
    }

    return ret == 0;
}

#endif
