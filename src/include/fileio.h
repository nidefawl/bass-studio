#pragma once
#include "str_util.h"
#include <fstream>
#include <utility>
#include <vector>
#include <cstdint>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#define shareMode _access_s
#else
#include <unistd.h>
#define shareMode access
#endif
#include "logging.h"

struct ImageBuf {
    std::vector<uint8_t> bytes;
    int w = 0;
    int h = 0;
    int bitdepth = 0;
};

struct SupportedFileType {
    String desc;
    String ext;
};

struct FileFound {
    String path;
    String name;
    String ext;
};
#ifdef _WIN32
#define DAW_FILEIO_PATHSEP "\\"
#else
#define DAW_FILEIO_PATHSEP "/"
#endif

using ByteBuf = std::vector<uint8_t>;

class window_base;

int32_t WriteFileVector(const String& filename, std::vector<uint8_t>& writebuffer);
void ReadFileVector(const String& filename, std::vector<uint8_t>& out);
int64_t ReadFileText(const String& filename, String& out, int resourceType = 0);
int promptUserFilePath(window_base* w, int mode, std::vector<SupportedFileType> fileTypes, String& _out);
int browseForFolder(const String& title, const String& pathStart, String& _out);

size_t GetFileSizeSafe(const String& filename);

inline void writeStringStream(const String& path, Stringstream& sstream) {
    Stringstream::pos_type len = sstream.tellp();
    std::vector<uint8_t> buf(len);
    buf.assign(std::istreambuf_iterator<char>(sstream), std::istreambuf_iterator<char>());
    WriteFileVector(path, buf);
}

inline bool FileExists(const String& Filename) {
    return shareMode(Filename.c_str(), 0) == 0;
}

bool CreateDirectoryIfNotExists(const String& DirPath);

inline int64_t FileSize(const String& fileName) {
    std::ifstream file(fileName.c_str(), std::ifstream::in | std::ifstream::binary);

    if (!file.is_open()) {
        return -1;
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.close();

    return (int64_t) fileSize;
}

int64_t ReadImage(const String& Filename, ImageBuf& ref);
inline int64_t ReadFileFully(const String& Filename, ByteBuf& ref) {
    if (FileExists(Filename)) {
        int64_t size = FileSize(Filename);
        if (size > 0) {
            std::ifstream file(Filename.c_str(), std::ifstream::in | std::ifstream::binary);
            if (file) {
                ref.reserve(size);
                ref.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            }
            size_t cur = file.tellg();
            if (size != (int64_t) cur) {
                my_printf("read %d bytes, expected %d bytes, BAD!\n", cur, size);
            } else {

                return size;
            }
        }
    }
    return -1;
}

inline void SplitPath(const String& in, String* path, String* name, String* ext, String* nameExt = nullptr) {
    std::size_t pathSep = in.find_last_of("/\\");
    String _nameExt;
    if (pathSep == String::npos) {
        _nameExt = in;
    } else {
        _nameExt = pathSep + 1 < in.length() ? in.substr(pathSep + 1) : "";
    }
    if (path != nullptr) {
        if (pathSep == String::npos) {
            *path = "";
        } else {
            *path = in.substr(0, pathSep);
        }
    }
    std::size_t fileExtSep = _nameExt.find_last_of('.');
    if (name != nullptr) {
        if (fileExtSep == String::npos || fileExtSep < 1) {
            *name = _nameExt;
        } else {
            *name = _nameExt.substr(0, fileExtSep);
        }
    }
    if (ext != nullptr) {
        if (fileExtSep == String::npos || fileExtSep >= _nameExt.length() - 1) {
            *ext = "";
        } else {
            *ext = _nameExt.substr(fileExtSep + 1);
        }
    }
    if (nameExt != nullptr) {
        *nameExt = _nameExt;
    }
}
inline String FileNameFromPath(const String& in) {
    String fileName;
    SplitPath(in, nullptr, nullptr, nullptr, &fileName);
    return fileName;
}

void findFilesWithExt(
        const String& strPath,
        const String& strExt,
        const bool& bRecursive,
        std::vector<FileFound>& _out, int depth = 0);

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
