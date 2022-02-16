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
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <fts.h>

#ifdef __linux__
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>


Display* getX11Display();
Window getX11FromWindowBase(window_base* w);


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
            int ret = close(handle);
            dbgassert(ret == 0);
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

int32_t WriteFileVector(const String& filename, vector<uint8_t>& writebuffer) {
    File fobj(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    ssize_t written = 0;
    while (written < writebuffer.size()) {
        ssize_t len = write(fobj.GetHandle(), writebuffer.data(), writebuffer.size());
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
    return (int32_t) written;
}

void ReadFileVector(const String& filename, vector<uint8_t>& out) {
    File fobj(filename, O_RDONLY, 0);
    size_t filesize   = GetFileSizeSafe(filename);
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
    std::string myString = "asoidfj";
    const char* hardcoded = "joiasijfaoiesjf";
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
        while (child && !(child->fts_info & FTS_DP)) {
            String fileName, ext;
            SplitPath(child->fts_name, nullptr, nullptr, &ext, &fileName);
            if (ext == strExt) {
                String path = String(child->fts_path) + child->fts_name;
                const FileFound f = { std::move(path), child->fts_name, ext };
                _out.push_back(f);
            }
            child = child->fts_link;
        }
        if (!bRecursive)
            break;
    }

    fts_close(file_system);
}


class FileTimeGetter::Impl {
    struct stat fStat;
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
    Impl(String path) {
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
private:
    FILE* m_handle;

    // Declared but not defined, to avoid double closing.
    FileImpl& operator=(const FileImpl&);
    FileImpl(const FileImpl&);

public:
    explicit FileImpl(const String& filename, OpenFileMode mode) {
        String strFileOpenMode = "";
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
        m_handle = fopen64(filename.c_str(), strFileOpenMode.c_str());

        ThrowLastErrorIf(m_handle == NULL,
                         "fopen64 call failed on file named " + filename);
    }

    ~FileImpl() { fclose(m_handle); }

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
        fwrite(data, len, 1, impl->GetHandle());
    }
}
void IOFile::flush() {
    if (this->validHandle) {
        fflush(impl->GetHandle());
    }
}

IOFile* IOFile::openFile(const String& filename, OpenFileMode mode) {
    FileImpl* impl = new FileImpl(filename, mode);
    if (!impl->GetHandle()) {
        delete impl;
        return nullptr;
    }
    return new IOFile(impl);
}

bool CreateDirectoryIfNotExists(const String& DirPath) {
    int mkdRet = mkdir(StringAsCStr(DirPath), 0755);
    ThrowLastErrorIf((mkdRet != 0) && (errno != EEXIST),
                     "mkdir call failed on file named " + DirPath);
    return true;
}

#endif
