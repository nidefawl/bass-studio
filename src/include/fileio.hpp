#pragma once
#include "str_util.hpp"
#include <fstream>
#include <utility>
#include <vector>
#include "types.hpp"
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define shareMode _access_s
#else
#include <unistd.h>
#define shareMode access
#endif
#include "logging.hpp"
#include "platform.hpp"
#include "assert_dbg.h"

struct ImageBuf {
    std::vector<uint8_t> bytes;
    int w        = 0;
    int h        = 0;
    int bitdepth = 0;
};

struct SupportedFileType {
    const char* desc;
    const char* ext;
};

struct SupportedFileTypes {
    const char* desc;
    std::vector<SupportedFileType> types;
};

struct FileFound {
    String path;
    String name;
    String ext;
    bool bIsDir = false;
    int32_t depth = -1; // -1 equals not set
};

using ByteBuf = std::vector<uint8_t>;

class window_base;

void RevealInExplorer(const String& path);
int32_t WriteFileVector(const String& filename, const std::vector<uint8_t>& writebuffer);
void ReadFileVector(const String& filename, std::vector<uint8_t>& out);
int64_t ReadFileText(const String& filename, String& out, int resourceType = 0);
int promptUserFilePath(window_base* w, int mode, SupportedFileTypes fileTypes, String& _out, String _defaultPath = "", String _defaultFilename = "");
int browseForFolder(const String& title, const String& pathStart, String& _out);

size_t GetFileSizeSafe(const String& filename);

inline void writeStringStream(const String& path, Stringstream& sstream) {
    std::vector<uint8_t> buf(sstream.tellp());
    buf.assign(std::istreambuf_iterator<char>(sstream), std::istreambuf_iterator<char>());
    WriteFileVector(path, buf);
}

bool FileExists(const String& Filename);

bool CreateDirectoryIfNotExists(const String& DirPath);
bool PathIsDirectory(const String& path);
bool DeleteDirectory(const String& DirPath, bool bRecursive = false);
bool DeleteAbsoluteFile(const String& FilePath);
bool MoveAbsoluteFile(const String& src, const String& dst);

inline int64_t FileSize(const String& fileName) {
    std::ifstream file(fileName.c_str(), std::ifstream::in | std::ifstream::binary);

    if (!file.is_open()) {
        return -1;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();

    return static_cast<int64_t>(fileSize);
}

int64_t ReadImage(const String& Filename, ImageBuf& ref);
int64_t ReadImageFromBuffer(const ByteBuf& Buffer, ImageBuf& ref);
inline int64_t ReadFileFully(const String& Filename, ByteBuf& ref) {
    if (FileExists(Filename)) {
        int64_t size = FileSize(Filename);
        if (size >= 0) {
            std::ifstream file(StringAsCStr(Filename), std::ifstream::in | std::ifstream::binary);
            if (file) {
                ref.reserve(size);
                ref.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            }
            dbgassert(size == static_cast<int64_t>(file.tellg()));
            return size;
        }
    }
    return -1;
}

inline void SplitPath(const String& in, String* path, String* name, String* ext, String* nameExt = nullptr) {
    String pathCopy = in;
    App::Platform::sanitizePathToFile(pathCopy);
    std::size_t pathSep = pathCopy.find_last_of(FILE_PATHSEP_CHAR);
    String _nameExt;
    if (pathSep == String::npos) {
        _nameExt = in;
    } else {
        _nameExt = pathSep + 1 < in.length() ? in.substr(pathSep + 1) : "";
    }
    if (path) {
        if (pathSep == String::npos) {
            *path = "";
        } else {
            *path = in.substr(0, pathSep);
        }
    }
    if (name || ext) {
        // We don't want to cut at the dot if the path names an existing directory
        bool bIsDir = PathIsDirectory(in);
        std::size_t fileExtSep = !bIsDir ? _nameExt.find_last_of('.') : String::npos;
        if (name) {
            if (fileExtSep == String::npos || fileExtSep < 1) {
                *name = _nameExt;
            } else {
                *name = _nameExt.substr(0, fileExtSep);
            }
        }
        if (!bIsDir && ext) {
            if (fileExtSep == String::npos || fileExtSep >= _nameExt.length() - 1) {
                *ext = "";
            } else {
                *ext = _nameExt.substr(fileExtSep + 1);
            }
        }
    }
    if (nameExt) {
        *nameExt = _nameExt;
    }
}
inline void SplitPathWide(const WString& in, WString* path, WString* name, WString* ext, WString* nameExt = nullptr) {
    WString pathCopy = in;
    App::Platform::sanitizePathToFileWide(pathCopy);
    std::size_t pathSep = pathCopy.find_last_of(FILE_PATHSEP_CHAR);
    WString _nameExt;
    if (pathSep == WString::npos) {
        _nameExt = in;
    } else {
        _nameExt = pathSep + 1 < in.length() ? in.substr(pathSep + 1) : L"";
    }
    if (path) {
        if (pathSep == WString::npos) {
            *path = L"";
        } else {
            *path = in.substr(0, pathSep);
        }
    }
    // TODO: We don't want to cut at the dot if the path names a directory
    std::size_t fileExtSep = _nameExt.find_last_of('.');
    if (name) {
        if (fileExtSep == WString::npos || fileExtSep < 1) {
            *name = _nameExt;
        } else {
            *name = _nameExt.substr(0, fileExtSep);
        }
    }
    if (ext) {
        if (fileExtSep == WString::npos || fileExtSep >= _nameExt.length() - 1) {
            *ext = L"";
        } else {
            *ext = _nameExt.substr(fileExtSep + 1);
        }
    }
    if (nameExt) {
        *nameExt = _nameExt;
    }
}
inline String FileNameFromPath(const String& in) {
    String fileName;
    SplitPath(in, nullptr, nullptr, nullptr, &fileName);
    return fileName;
}

enum list_dir_flags_e {
    LIST_DIR_RECURSIVE = 1 << 0,
    LIST_DIR_DIRS = 1 << 1,
    LIST_DIR_EMPTY_DIRS = 1 << 2,
};

void listFilesystemNonRecursive(
        const String& strPath,
        const std::vector<String>& vecExt,
        std::vector<FileFound>& _out);
void findFilesWithExt(
        const String& strPath,
        const String& strExt,
        bool bRecursive,
        std::vector<FileFound>& _out);
void RevealInExplorer(const String& path);

class FileTimeGetter {
    class Impl;

public:
public:
    int64_t getWriteTimeI64();
    explicit FileTimeGetter(const String& path);
    ~FileTimeGetter();

private:
    Impl* m_impl;
};

class FileImpl;
enum class OpenFileMode {
    READ,
    WRITE,
    READWRITE
};
class IOFile {
private:
    FileImpl* impl;
    bool validHandle;
    explicit IOFile(FileImpl* _impl) noexcept;

public:
    ~IOFile();
    void write(const char* data, size_t len);
    void flush();
    bool isValid() const {
        return validHandle;
    }
    /** may return nullptr, will not throw and not indicate reason of failure **/
    static IOFile* openFile(const String& filename, OpenFileMode mode);
};
