#ifdef _WIN32
#include "str_util.h"
#include "fileio.h"
#include "exceptions.h"
#include "types.h"
#include <windows.h>
#include <vector>
#include <limits>
#include <stdexcept>
#include "assert_dbg.h"
#include "platform.h"
#include "platform_win.h"
#include <shlobj.h>

bool CreateDirectoryIfNotExists(const String& DirPath) {
    String partPath = "";
    do {
        auto pos = DirPath.find_first_of('\\', partPath.size());
        if (pos == String::npos) {
            pos = DirPath.size();
        }
        partPath = DirPath.substr(0, pos + 1);
        if (partPath.empty()) {
            break;
        }
        if (!FileExists(partPath)) {
            if (0 == CreateDirectoryA(StringAsCStr(partPath), nullptr)) {
                return false;
            }
        }
    } while (partPath.size() < DirPath.size());
    return true;
}

bool DeleteAbsoluteFile(const String& FilePath) {
    return 0 != DeleteFileA(StringAsCStr(FilePath));
}

bool MoveAbsoluteFile(const String& src, const String& dst) {
    return 0 != MoveFileA(StringAsCStr(src), StringAsCStr(dst));
}

bool PathIsDirectory(const String& path) {
    DWORD dwAttrib = GetFileAttributesA(StringAsCStr(path));
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool DeleteDirectory(const String& DirPath, bool bRecursive) {
    if (!assert_expr(!DirPath.empty())) {
        return false;
    }
    if (bRecursive) {
        SHFILEOPSTRUCTA fileOp;
        fileOp.hwnd   = nullptr;
        fileOp.wFunc  = FO_DELETE;
        fileOp.pFrom  = StringAsCStr(DirPath + "\\*");
        fileOp.pTo    = nullptr;
        fileOp.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;
        fileOp.fAnyOperationsAborted = FALSE;
        fileOp.lpszProgressTitle     = nullptr;
        return SHFileOperationA(&fileOp) == 0;
    } else {
        // delete contents first
        WIN32_FIND_DATAA findFileData;
        HANDLE hFind = FindFirstFileA(StringAsCStr(DirPath + "\\*"), &findFileData);
        if (hFind == INVALID_HANDLE_VALUE) {
            return false;
        }
        do {
            if (findFileData.cFileName[0] == '.' && (findFileData.cFileName[1] == 0 || (findFileData.cFileName[1] == '.' && findFileData.cFileName[2] == 0))) {
                continue;
            }
            String filePath = DirPath + "\\" + findFileData.cFileName;
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!DeleteDirectory(filePath, true)) {
                    return false;
                }
            } else {
                if (0 == DeleteFileA(StringAsCStr(filePath))) {
                    return false;
                }
            }
        } while (FindNextFileA(hFind, &findFileData) != 0);
        return RemoveDirectoryA(StringAsCStr(DirPath)) != 0;
    }
}


void ThrowLastErrorIf(bool expression, const String& msg) {
    if (expression) {
        throw FileIOException(GetLastError(), msg);
    }
}

class FileImpl {
private:
    HANDLE m_handle;

public:
    // avoid double closing.
    FileImpl& operator=(const FileImpl&) = delete;
    FileImpl& operator=(FileImpl&&) = delete;
    FileImpl(const FileImpl&) = delete;
    FileImpl(FileImpl&&) = delete;
    explicit FileImpl(const String& filename, OpenFileMode mode) {
        int createFlags, attr, shareMode;
        int accessMode;
        switch (mode) {
            case OpenFileMode::READ:
                createFlags = OPEN_EXISTING;
                attr        = FILE_ATTRIBUTE_NORMAL;
                shareMode   = FILE_SHARE_READ;
                accessMode  = GENERIC_READ;
                break;
            case OpenFileMode::WRITE:
                createFlags = CREATE_ALWAYS;
                attr        = FILE_ATTRIBUTE_NORMAL;
                shareMode   = 0;//exclusive
                accessMode  = GENERIC_WRITE;
                break;
            case OpenFileMode::READWRITE:
                createFlags = CREATE_ALWAYS;
                attr        = FILE_ATTRIBUTE_NORMAL;
                shareMode   = FILE_SHARE_READ;
                accessMode  = GENERIC_READ | GENERIC_WRITE;
                break;
            default:
                throw appexception("Invalid file open mode");
        }
        m_handle = CreateFileA(filename.c_str(), accessMode, shareMode,
                               nullptr, createFlags, attr, nullptr);
        ThrowLastErrorIf(m_handle == INVALID_HANDLE_VALUE,
                         "CreateFile call failed on file named " + filename);
    }

    ~FileImpl() { CloseHandle(m_handle); }

    HANDLE GetHandle() { return m_handle; }
};

size_t GetFileSizeSafe(const String& filename) {
    FileImpl fobj(filename, OpenFileMode::READ);
    LARGE_INTEGER filesize;

    BOOL result = GetFileSizeEx(fobj.GetHandle(), &filesize);
    ThrowLastErrorIf(result == FALSE, "GetFileSizeEx failed: " + filename);

    if (filesize.QuadPart < (std::numeric_limits<int64_t>::max)()) {
        return filesize.QuadPart;
    } else {
        throw;
    }
}

int32_t WriteFileVector(const String& filename, const std::vector<uint8_t>& writebuffer) {
    FileImpl fobj(filename, OpenFileMode::WRITE);
    DWORD bytesWrite = 0;
    BOOL result = WriteFile(fobj.GetHandle(), writebuffer.data(), static_cast<DWORD>(writebuffer.size()), &bytesWrite, nullptr);
    ThrowLastErrorIf(result == FALSE, "WriteFile failed: " + filename);
    return (int32_t) bytesWrite;
}

void ReadFileVector(const String& filename, std::vector<uint8_t>& out) {
    FileImpl fobj(filename, OpenFileMode::READ);
    size_t filesize = GetFileSizeSafe(filename);
    DWORD bytesRead = 0;
    out.resize(filesize);
    BOOL result = ReadFile(fobj.GetHandle(), out.data(), static_cast<DWORD>(filesize), &bytesRead, nullptr);
    ThrowLastErrorIf(result == FALSE, "ReadFile failed: " + filename);
}

void findFilesWithExtRecursive(
        const String& strPath,
        const std::vector<String>& vecExt,
        const bool bRecursive, const bool bIncludeDirs,
        std::vector<FileFound>& _out, int depth) {
    WIN32_FIND_DATA file;

    String strSearchPath = strPath;
    App::Platform::sanitizePathToDirectory(strSearchPath);
    String findPattern = strSearchPath + "*";

    HANDLE hFile = FindFirstFile(findPattern.c_str(), &file);
    if (hFile != INVALID_HANDLE_VALUE) {
        std::vector<String> subDirs;

        do {
            String curFilePath;
            curFilePath = file.cFileName;

            if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if ((curFilePath != ".") && (curFilePath != "..")) {
                    if (bRecursive) {
                        subDirs.push_back(strSearchPath + curFilePath);
                    }
                    if (bIncludeDirs) {
                        String absDirPath = strSearchPath + curFilePath;
                        App::Platform::sanitizePathToDirectory(absDirPath);
                        const FileFound f = { absDirPath, curFilePath, "", true };
                        _out.push_back(f);
                    }
                }
            } else {
                curFilePath = strSearchPath + file.cFileName;
                String fileName, ext;
                SplitPath(curFilePath, nullptr, nullptr, &ext, &fileName);
                if (std::find(vecExt.cbegin(), vecExt.cend(), ext) != vecExt.cend()) {
                    String absFilePath = strSearchPath + file.cFileName;
                    App::Platform::sanitizePathToFile(absFilePath);
                    const FileFound f = { absFilePath, fileName, ext, false };
                    _out.push_back(f);
                }
            }
        } while (FindNextFile(hFile, &file));

        FindClose(hFile);


        if (!subDirs.empty()) {
            for (String& s : subDirs) {
                findFilesWithExtRecursive(s, vecExt, bRecursive, bIncludeDirs, _out, depth + 1);
            }
        }
    }
}

void findFilesWithExt(
        const String& strPath,
        const String& strExt,
        bool bRecursive,
        std::vector<FileFound>& _out) {
    findFilesWithExtRecursive(strPath, {strExt}, bRecursive, false, _out, 0);
}
void listDirectoryFiles(
        const String& strPath,
        const std::vector<String>& vecExt,
        std::vector<FileFound>& _out) {
    findFilesWithExtRecursive(strPath, vecExt, false, true, _out, 0);
}

class FileTimeGetter::Impl {
public:
    FILETIME ftCreate = {};
    FILETIME ftAccess = {};
    FILETIME ftWrite  = {};
    HANDLE hFile      = {};
    bool ok           = false;

public:
    int64_t getWriteTimeI64() const {
        if (!ok) {
            return 0;
        }
        uint64_t time = (uint64_t) ftWrite.dwLowDateTime | (uint64_t) ftWrite.dwHighDateTime << 32;
        return (int64_t)time;
    }
    explicit Impl(const String& path) {
        hFile = CreateFile(StringAsCStr(path), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            ok = GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite);
        }
    }
    ~Impl() {
        if (hFile != INVALID_HANDLE_VALUE)
            CloseHandle(hFile);
    }
};
FileTimeGetter::FileTimeGetter(const String& path)
    : m_impl{ new FileTimeGetter::Impl{ path } } {
}
FileTimeGetter::~FileTimeGetter() {
    delete m_impl;
}
int64_t FileTimeGetter::getWriteTimeI64() {
    return m_impl->getWriteTimeI64();
}

IOFile::IOFile(FileImpl* _impl) noexcept : impl(_impl) {
    dbgassert(impl->GetHandle());
    this->validHandle = true;
}
IOFile::~IOFile() {
    dbgassert(impl->GetHandle());
    delete impl;
}
void IOFile::write(const char* data, size_t len) {
    DWORD bytesWrite = 0;
    BOOL result      = WriteFile(impl->GetHandle(), data, static_cast<DWORD>(len), &bytesWrite, nullptr);
    if (!result) {
        validHandle = false;
    }
}
void IOFile::flush() {
    dbgassert(impl->GetHandle());
    if (!FlushFileBuffers(impl->GetHandle())) {
        validHandle = false;
    }
}

IOFile* IOFile::openFile(const String& filename, OpenFileMode mode) {
    try {
        auto* impl = new FileImpl(filename, mode); // throws
        auto* iofile = new IOFile(impl); // noexcept
        return iofile;
    } catch (const FileIOException& e) {
        log_printf("IOFile::openFile File IO exception: %s (%d)\n", e.what(), e.GetErrorCode());
    }
    return nullptr;
}

void RevealInExplorer(const String& _path) {
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
    ShellExecuteA(nullptr, "open", "explorer.exe", StringAsCStr("/select," + path), nullptr, SW_SHOWNORMAL);
}
#endif // _WIN32
