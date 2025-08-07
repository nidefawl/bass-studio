#if defined(__linux__) || defined(__APPLE__)
#include "fileio.hpp"
#include "exceptions.hpp"
#include "str_util.hpp"
#include "assert_dbg.h"
#include "window.hpp"
#include "platform.hpp"
#include "logging.hpp"

#include <stb/stb_image.h>
#include <vector>
#include <iostream>
#include <string>

#include <limits>
#include <cstdio>
#include "types.hpp"
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
                strFileOpenMode = "w+b"; // Open for reading and writing, truncating the file to zero length
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

size_t GetFileSizeSafe(const String& filename) {
    struct stat fStat{};
    if (stat(StringAsCStr(filename), &fStat) == 0) {
        return fStat.st_size;
    }
    return 0;
}

int32_t WriteFileVector(const String& filename, const std::vector<uint8_t>& writebuffer) {
    FileImpl fobj(filename, OpenFileMode::WRITE);
    const auto bufferSize = writebuffer.size();
    size_t written = fwrite(writebuffer.data(), 1, bufferSize, fobj.GetHandle());
    if (written != bufferSize) {
        throw FileIOException(errno, "WriteFile failed: " + filename);
    }
    return static_cast<int32_t>(written);
}

void ReadFileVector(const String& filename, std::vector<uint8_t>& out) {
    FileImpl fobj(filename, OpenFileMode::READ);
    auto filesize = GetFileSizeSafe(filename);
    
    out.resize(filesize);
    
    size_t bytesRead = fread(out.data(), 1, filesize, fobj.GetHandle());
    if (bytesRead != filesize) {
        throw FileIOException(errno, "ReadFile failed: " + filename);
    }
}

void findFilesWithExtList(
        const String& strPath,
        const std::vector<String>& vecExt,
        const bool bRecursive,
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
        if (fs_entry->fts_info == FTS_DP)
            continue;
        FTSENT* child = fts_children(file_system, 0);
        for (; child; child = child->fts_link) {
            if (child->fts_info == FTS_D || child->fts_info == FTS_DP || child->fts_info == FTS_DNR) {
                continue; // skip directories
            }
            if (child->fts_info == FTS_DC) {
                continue; // skip cycles
            }
            if (child->fts_info == FTS_ERR) {
                continue;
            }
            if (child->fts_info == FTS_NS) {
                continue; // skip unreadable files
            }

            String fileName, ext;
            String path = String(child->fts_path) + child->fts_name;
            SplitPath(path, nullptr, nullptr, &ext, &fileName);
            if (vecExt.empty() || std::find(vecExt.cbegin(), vecExt.cend(), ext) != vecExt.cend()) {
                App::Platform::sanitizePathToFile(path);
                const FileFound f = { std::move(path), child->fts_name, ext, bool(child->fts_info & FTS_D) };
                _out.push_back(f);
            }
        }
        if (!bRecursive)
            break;
    }

    fts_close(file_system);
}

void findDirectoriesWithExt(
        const String& strPath,
        const String& ext,
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
        if (fs_entry->fts_info != FTS_D) {
            continue; // skip non directories
        }
        String fileName, fileExt;
        SplitPath(fs_entry->fts_name, nullptr, nullptr, &fileExt, &fileName);
        if (ext == fileExt) {
            String path = String(fs_entry->fts_path);
            // App::Platform::sanitizePathToDirectory(path);
            const FileFound f = { std::move(path), fs_entry->fts_name, ext, true, -1 };
            _out.push_back(f);
        }
    }

    fts_close(file_system);
}

void findFilesWithExt(
        const String& strPath,
        const String& strExt,
        bool bRecursive,
        std::vector<FileFound>& _out) {
    findFilesWithExtList(strPath, { strExt }, bRecursive, _out);
}

void listFilesystemNonRecursive(
        const String& strPath,
        const std::vector<String>& vecExt,
        std::vector<FileFound>& _out) {
    auto path = strPath;
    App::Platform::sanitizePathToDirectory(path);
    DIR* d = opendir(path.c_str());
    if (d == NULL) {
        log_lf(Log::L_WARN, "Could not open directory: %s\n", StringAsCStr(path));
        return;
    }
    struct dirent* fs_entry = nullptr;
    while ((fs_entry = readdir(d))) {
        bool bIsDir  = fs_entry->d_type == DT_DIR;
        bool bIsFile = fs_entry->d_type == DT_REG;
        if (bIsDir && fs_entry->d_name[0] != '.') {
            auto pathDir = path + fs_entry->d_name;
            App::Platform::sanitizePathToDirectory(pathDir);
            const FileFound f = { std::move(pathDir), fs_entry->d_name, "", true };
            _out.push_back(f);
        } else if (bIsFile) {
            String ext;
            SplitPath(fs_entry->d_name, nullptr, nullptr, &ext, nullptr);
            if (vecExt.empty() || std::find(vecExt.cbegin(), vecExt.cend(), ext) != vecExt.cend()) {
                auto pathFile = path + fs_entry->d_name;
                App::Platform::sanitizePathToFile(pathFile);
                const FileFound f = { std::move(pathFile), fs_entry->d_name, ext, false };
                _out.push_back(f);
            }
        }
    }
    closedir(d);// finally close the directory
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

bool DeleteAbsoluteFile(const String& FilePath) {
    return 0 != remove(StringAsCStr(FilePath));
}

bool MoveAbsoluteFile(const String& src, const String& dst) {
    return 0 != rename(StringAsCStr(src), StringAsCStr(dst));
}

bool PathIsDirectory(const String& path) {
    struct stat statbuf{};
    if (stat(StringAsCStr(path), &statbuf) != 0) {
        return false;
    }
    return S_ISDIR(statbuf.st_mode);
}

void RevealInExplorer(const String& _path) {
    if (system("which xdg-open > /dev/null") != 0) {
        log_lf(Log::L_WARN, "xdg-open not found, cannot reveal in explorer\n");
        return;
    }
    
    String path = _path;
    App::Platform::sanitizePathToFile(path);
    if (path.empty()) {
        return;
    }
    // check if file exists
    const bool bExists = FileExists(path);
    // if not, pick parent folder
    if (!bExists) {
        String parentPath;
        SplitPath(path, &parentPath, nullptr, nullptr);
        if (FileExists(parentPath)) {
            path = parentPath;
        } else {
            return;
        }
    }
    String cmd = "xdg-open " + path;
    system(cmd.c_str());
}

bool FileExists(const String& Filename) {
    return shareMode(Filename.c_str(), 0) == 0;
}

#endif
