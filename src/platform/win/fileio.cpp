#ifdef _WIN32
#include "fileio.h"
#include "exceptions.h"
#include <stb_image.h>
#include <Windows.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <string>
#include <limits>
#include <stdexcept>
#include <stdint.h>

HWND getMainHWND(); // window.cpp

int64_t ReadImage( const String &Filename, ImageBuf& ref)
{
	 unsigned char *data = stbi_load(StringAsCStr(Filename), &ref.w, &ref.h, &ref.bitdepth, 0);
	 int64_t bufSize = -1;
	 if (data) {
		 bufSize = ref.w*ref.h*ref.bitdepth;
		 ref.bytes.reserve(bufSize);
		 ref.bytes.assign(data, data+bufSize);
	 }
	 stbi_image_free(data);
	 return bufSize;
}

using namespace std;

String FormatErrorMessage(int32_t error, String msg)
{
	static const int BUFFERLENGTH = 1024;
	vector<char> buf(BUFFERLENGTH);
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, error, 0, buf.data(),
		BUFFERLENGTH - 1, 0);
	if (msg.empty())
		return String(buf.data());
	return String(buf.data()) + "   (" + msg + ")";
}


void ThrowLastErrorIf(bool expression, const String& msg)
{
	if (expression) {
		throw FileIOException(GetLastError(), msg);
	}
}

class File
{
private:
	HANDLE m_handle;

	// Declared but not defined, to avoid double closing.
	File& operator=(const File&);
	File(const File&);
public:
	explicit File(const String& filename, int mode)
	{
		int flags = OPEN_EXISTING;
		int attr = FILE_ATTRIBUTE_NORMAL;
		int access = FILE_SHARE_READ;
		if (mode == GENERIC_WRITE) {
			flags = CREATE_ALWAYS;
			attr = FILE_ATTRIBUTE_NORMAL;
			access = 0; //exclusive
		}
		m_handle = CreateFileA(filename.c_str(), mode, access,
			nullptr, flags, attr, nullptr);
		ThrowLastErrorIf(m_handle == INVALID_HANDLE_VALUE,
			"CreateFile call failed on file named " + filename);
	}

	~File() { CloseHandle(m_handle); }

	HANDLE GetHandle() { return m_handle; }
};

size_t GetFileSizeSafe(const String& filename)
{
	File fobj(filename, GENERIC_READ);
	LARGE_INTEGER filesize;

	BOOL result = GetFileSizeEx(fobj.GetHandle(), &filesize);
	ThrowLastErrorIf(result == FALSE, "GetFileSizeEx failed: " + filename);

	if (filesize.QuadPart < (numeric_limits<size_t>::max)()) {
		return filesize.QuadPart;
	}
	else {
		throw;
	}
}

int32_t WriteFileVector(const String& filename, vector<uint8_t>& writebuffer)
{
	File fobj(filename, GENERIC_WRITE);
	DWORD bytesWrite = 0;

	BOOL result = WriteFile(fobj.GetHandle(), writebuffer.data(), writebuffer.size(),
		&bytesWrite, nullptr);
	ThrowLastErrorIf(result == FALSE, "WriteFile failed: " + filename);

	cout << filename << "bytesWrite: " << bytesWrite << endl;

	return (int32_t) bytesWrite;
}

void ReadFileVector(const String& filename, vector<uint8_t>& out)
{
	File fobj(filename, GENERIC_READ);
	size_t filesize = GetFileSizeSafe(filename);
	DWORD bytesRead = 0;

	out.resize(filesize);

	BOOL result = ReadFile(fobj.GetHandle(), out.data(), filesize, &bytesRead, nullptr);
	ThrowLastErrorIf(result == FALSE, "ReadFile failed: " + filename);

	cout << filename << " file size: " << filesize << ", bytesRead: " << bytesRead << endl;
	cout << filename << " out.size: " << out.size() << endl;

}
int promptUserFilePath(window_base* w, int mode, std::vector<SupportedFileType> fileTypes, String& _out) {
//	const char supportedFiles = "Text Files (*." fileExt ")\0*." fileExt "\0All Files (*.*)\0*.*\0";
	char supportedFiles[MAX_PATH] = "";
	int offset = 0;
	fileTypes.push_back(SupportedFileType{"All Files", "*"});
	for (SupportedFileType& type : fileTypes) {
		offset += _snprintf(supportedFiles+offset, MAX_PATH-offset, "%s (*.%s)", StringAsCStr(type.desc), StringAsCStr(type.ext));
		supportedFiles[offset] = 0;
		offset++;
		offset += _snprintf(supportedFiles+offset, MAX_PATH-offset, "*.%s", StringAsCStr(type.ext));
		supportedFiles[offset] = 0;
		offset++;
	}
	supportedFiles[offset] = 0;
	offset++;
	if (mode == 0) {

		OPENFILENAME ofn;
		char szFileName[MAX_PATH] = "";

		ZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = getMainHWND();
		ofn.lpstrFilter = supportedFiles;
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		ofn.lpstrDefExt = StringAsCStr(fileTypes[0].ext);

		if (GetOpenFileName(&ofn)) {
			_out = szFileName;
			return 1;
		}
	}
	if (mode == 1) {

		OPENFILENAME ofn;
		char szFileName[MAX_PATH] = "";

		ZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = getMainHWND();
		ofn.lpstrFilter = supportedFiles;
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAX_PATH;
	    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		ofn.lpstrDefExt = StringAsCStr(fileTypes[0].ext);

		if (GetSaveFileName(&ofn)) {
			_out = szFileName;
			return 1;
		}
	}
	return 0;
}


void findFilesWithExt(
		const String& strPath,
		const String& strExt,
		const bool& bRecursive,
		std::vector<FileFound>& _out, int depth)
{
    WIN32_FIND_DATA file;

    String strSearchPath = strPath;
    if (!strSearchPath.empty()) {
    	size_t end = strSearchPath.size() - 1;
    	char last = strSearchPath[end];
    	if (last != '/' && last != '\\') {
    		strSearchPath += "/";
    	}
	}
    String findPattern = strSearchPath + "*";
    if (depth == 0)
    my_printf("findPattern '%s'\n", findPattern.c_str());

    HANDLE hFile = FindFirstFile(findPattern.c_str(), &file);
    if (hFile != INVALID_HANDLE_VALUE)
    {
		std::vector<String> subDirs;

		do {
			String curFilePath = file.cFileName;

			if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if ((curFilePath != ".") && (curFilePath != "..") && (bRecursive)) {
					subDirs.push_back(strSearchPath + curFilePath);
				}
			} else {
				curFilePath = strSearchPath + file.cFileName;
				String fileName, ext;
				SplitPath(curFilePath, NULL, NULL, &ext, &fileName);
				if (ext == strExt) {

					const FileFound f = {curFilePath, fileName, ext};
					_out.push_back(f);
				}

			}
		} while (FindNextFile(hFile, &file));

		FindClose(hFile);


		if (!subDirs.empty()) {
			for (String& s : subDirs) {
				findFilesWithExt(s, strExt, bRecursive, _out, depth+1);
			}
		}
	}
}

class FileTimeGetter::Impl {
public:
    FILETIME ftCreate = {0};
    FILETIME ftAccess = {0};
    FILETIME ftWrite = {0};
    HANDLE hFile = {0};
    bool ok = false;
public:
    int64_t getWriteTimeI64() {
    	if (!ok) {
    		return 0;
    	}
    	int64_t time = (uint64_t)ftWrite.dwLowDateTime;
    	time = (uint64_t)time | (uint64_t)ftWrite.dwHighDateTime << 32;
    	return time;
    }
    Impl(String path) {
	    hFile = CreateFile(StringAsCStr(path), GENERIC_READ, FILE_SHARE_READ, NULL,
	        OPEN_EXISTING, 0, NULL);
	    if(hFile != INVALID_HANDLE_VALUE)
	    {
	    	ok = GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite);
	    }

	}
	~Impl() {
	    if(hFile != INVALID_HANDLE_VALUE)
	    	CloseHandle(hFile);
	}
};
FileTimeGetter::FileTimeGetter(String path) : _M_Impl{new FileTimeGetter::Impl{path}} {

}
FileTimeGetter::~FileTimeGetter() {
	delete _M_Impl;
}
int64_t FileTimeGetter::getWriteTimeI64() {
	return _M_Impl->getWriteTimeI64();
}
#endif
