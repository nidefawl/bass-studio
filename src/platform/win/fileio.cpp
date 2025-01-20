#include "logging.h"
#include <cstdint>
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
#include "str_win32.h"
#include <shlobj.h>


bool FileExistsWide(const WString& Filename) {
    return _waccess_s(Filename.c_str(), 0) == 0;
}

bool FileExists(const String& Filename) {
    auto strWide = StringU8ToW(Filename);
    return _waccess_s(strWide.c_str(), 0) == 0;
}

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
            auto partPathW = StringU8ToW(partPath);
            if (0 == CreateDirectoryW(partPathW.c_str(), nullptr)) {
                return false;
            }
        }
    } while (partPath.size() < DirPath.size());
    return true;
}
void LogLastWin32Error(const String& msg) {
    DWORD err = GetLastError();
    log_lf(Log::L_ERROR, "%s: %s\n", StringAsCStr(msg), FormatErrorMessage(err).c_str());
}

bool MoveAbsoluteFile(const String& src, const String& dst) {
    auto srcW = StringU8ToW(src);
    auto dstW = StringU8ToW(dst);
    auto ret = MoveFileW(srcW.c_str(), dstW.c_str());
    if (ret == 0) {
        LogLastWin32Error("MoveFileA failed");
    }
    return ret;
}

bool DeleteAbsoluteFile(const String& FilePath) {
    auto filePathW = StringU8ToW(FilePath);
    auto ret = DeleteFileW(filePathW.c_str());
    if (ret == 0) {
        LogLastWin32Error("DeleteFileA failed");
    }
    return ret;
}

bool PathIsDirectory(const String& path) {
    auto filePathW = StringU8ToW(path);
    DWORD dwAttrib = GetFileAttributesW(StringAsCStr(filePathW));
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool DeleteDirectoryW(const WString& DirPathW, bool bRecursive) {
    if (!assert_expr(!DirPathW.empty())) {
        return false;
    }
    if (bRecursive) {
        SHFILEOPSTRUCTW fileOp;
        fileOp.hwnd   = nullptr;
        fileOp.wFunc  = FO_DELETE;
        fileOp.pFrom  = StringAsCStr(DirPathW + L"\\*");
        fileOp.pTo    = nullptr;
        fileOp.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;
        fileOp.fAnyOperationsAborted = FALSE;
        fileOp.lpszProgressTitle     = nullptr;
        return SHFileOperationW(&fileOp) == 0;
    } else {
        // delete contents first
        WIN32_FIND_DATAW findFileData;
        HANDLE hFind = FindFirstFileW(StringAsCStr(DirPathW + L"\\*"), &findFileData);
        if (hFind == INVALID_HANDLE_VALUE) {
            return false;
        }
        do {
            if (findFileData.cFileName[0] == '.' && (findFileData.cFileName[1] == 0 || (findFileData.cFileName[1] == '.' && findFileData.cFileName[2] == 0))) {
                continue;
            }
            auto filePath = DirPathW + L"\\" + findFileData.cFileName;
            if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!DeleteDirectoryW(filePath, true)) {
                    return false;
                }
            } else {
                if (0 == DeleteFileW(StringAsCStr(filePath))) {
                    return false;
                }
            }
        } while (FindNextFileW(hFind, &findFileData) != 0);
        return RemoveDirectoryW(StringAsCStr(DirPathW)) != 0;
    }
}
bool DeleteDirectory(const String& DirPath, bool bRecursive) {
    auto DirPathW = StringU8ToW(DirPath);
    return DeleteDirectoryW(DirPathW, bRecursive);
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
        auto strW = StringU8ToW(filename);
        m_handle = CreateFileW(strW.c_str(), accessMode, shareMode,
                               nullptr, createFlags, attr, nullptr);
        ThrowLastErrorIf(m_handle == INVALID_HANDLE_VALUE, "Failed to open file");
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
bool DirectoryHasOneOrMoreFiles(const WString& dirPath) {
    WIN32_FIND_DATAW file;
    WString strSearchPath = dirPath;
    App::Platform::sanitizePathToDirectoryWide(strSearchPath);
    WString findPattern = strSearchPath + L"*";

    HANDLE hFile = FindFirstFileW(findPattern.c_str(), &file);
    if (hFile != INVALID_HANDLE_VALUE) {
        do {
            WString curFilePath = file.cFileName;
            if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if ((curFilePath != L".") && (curFilePath != L"..")) {
                    FindClose(hFile);
                    return true;
                }
            } else {
                FindClose(hFile);
                return true;
            }
        } while (FindNextFileW(hFile, &file));
        FindClose(hFile);
    }
    return false;
}
void findFilesWithExtRecursive(
        const WString& strPath,
        const std::vector<WString>& vecExt,
        int32_t flags,
        std::vector<FileFound>& _out, int depth) {
    const bool bRecursive = flags & LIST_DIR_RECURSIVE;
    const bool bIncludeDirs = flags & LIST_DIR_DIRS;
    const bool bIncludeEmptyDirs = flags & LIST_DIR_EMPTY_DIRS;
    WIN32_FIND_DATAW file;

    WString strSearchPath = strPath;
    App::Platform::sanitizePathToDirectoryWide(strSearchPath);
    WString findPattern = strSearchPath + L"*";

    HANDLE hFile = FindFirstFileW(findPattern.c_str(), &file);
    if (hFile != INVALID_HANDLE_VALUE) {
        std::vector<WString> subDirs;

        do {
            WString curFilePath = file.cFileName;

            if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if ((curFilePath != L".") && (curFilePath != L"..")) {
                    if (bRecursive) {
                        subDirs.push_back(strSearchPath + curFilePath);
                    }
                    if (bIncludeDirs && (bIncludeEmptyDirs || DirectoryHasOneOrMoreFiles(strSearchPath + curFilePath))) {
                        auto absDirPath = strSearchPath + curFilePath;
                        App::Platform::sanitizePathToDirectoryWide(absDirPath);
                        const FileFound f = { StringWToU8(absDirPath), StringWToU8(curFilePath), "", true };
                        _out.push_back(f);
                    }
                }
            } else {
                curFilePath = strSearchPath + file.cFileName;
                WString fileName, ext;
                SplitPathWide(curFilePath, nullptr, nullptr, &ext, &fileName);
                if (vecExt.empty() || std::find(vecExt.cbegin(), vecExt.cend(), ext) != vecExt.cend()) {
                    auto absFilePath = strSearchPath + file.cFileName;
                    App::Platform::sanitizePathToFileWide(absFilePath);
                    const FileFound f = { StringWToU8(absFilePath), StringWToU8(fileName), StringWToU8(ext), false };
                    _out.push_back(f);
                }
            }
        } while (FindNextFileW(hFile, &file));

        FindClose(hFile);

        if (!subDirs.empty()) {
            for (auto& s : subDirs) {
                findFilesWithExtRecursive(s, vecExt, flags, _out, depth + 1);
            }
        }
    }
}

void findFilesWithExt(
        const String& strPath,
        const String& strExt,
        bool bRecursive,
        std::vector<FileFound>& _out) {
    int32_t flags = bRecursive ? LIST_DIR_RECURSIVE : 0;
    findFilesWithExtRecursive(StringU8ToW(strPath), {StringU8ToW(strExt)}, flags, _out, 0);
}

void listFilesystemNonRecursive(
        const String& strPath,
        const std::vector<String>& vecExt,
        std::vector<FileFound>& _out) {
    std::vector<WString> vecExtW;
    vecExtW.reserve(vecExt.size());
    for (const auto& ext : vecExt) {
        vecExtW.push_back(StringU8ToW(ext));
    }
    int32_t flags = LIST_DIR_DIRS | LIST_DIR_EMPTY_DIRS;
    findFilesWithExtRecursive(StringU8ToW(strPath), vecExtW, flags, _out, 0);
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
        auto strW = StringU8ToW(path);
        hFile = CreateFileW(strW.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
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
    WString path = StringU8ToW(_path);
    App::Platform::sanitizePathToFileWide(path);
    if (path.empty()) {
        return;
    }
    // check if file exists
    const bool bExists = FileExistsWide(path);
    // if not, pick parent folder
    if (!bExists) {
        WString parentPath;
        SplitPathWide(path, &parentPath, nullptr, nullptr);
        if (FileExistsWide(parentPath)) {
            path = parentPath;
        } else {
            return;
        }
    }
    ShellExecuteW(nullptr, L"open", L"explorer.exe", StringAsCStr(L"/select," + path), nullptr, SW_SHOWNORMAL);
}

#endif // _WIN32
