// compile with: /EHsc  
#include <Windows.h>  
#include <stdlib.h>  
#include <vector>  
#include <iostream>  
#include <string>  
#include <limits>  
#include <stdexcept>  

namespace {
using namespace std;

string FormatErrorMessage(DWORD error, const string& msg)
{
	static const int BUFFERLENGTH = 1024;
	vector<char> buf(BUFFERLENGTH);
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, error, 0, buf.data(),
		BUFFERLENGTH - 1, 0);
	return string(buf.data()) + "   (" + msg + ")";
}

class Win32Exception : public runtime_error
{
private:
	DWORD m_error;
public:
	Win32Exception(DWORD error, const string& msg)
		: runtime_error(FormatErrorMessage(error, msg)), m_error(error) { }

	DWORD GetErrorCode() const { return m_error; }
};

void _ThrowLastErrorIf(bool expression, const string& msg)
{
	if (expression) {
		throw Win32Exception(GetLastError(), msg);
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
	explicit File(const string& filename)
	{
		m_handle = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
		_ThrowLastErrorIf(m_handle == INVALID_HANDLE_VALUE,
			"CreateFile call failed on file named " + filename);
	}

	~File() { CloseHandle(m_handle); }

	HANDLE GetHandle() { return m_handle; }
};

size_t _GetFileSizeSafe(const string& filename)
{
	File fobj(filename);
	LARGE_INTEGER filesize;

	BOOL result = GetFileSizeEx(fobj.GetHandle(), &filesize);
	_ThrowLastErrorIf(result == FALSE, "GetFileSizeEx failed: " + filename);

	if (filesize.QuadPart < (numeric_limits<size_t>::max)()) {
		return filesize.QuadPart;
	}
	else {
		throw;
	}
}

vector<char> ReadFileVector(const string& filename)
{
	File fobj(filename);
	size_t filesize = _GetFileSizeSafe(filename);
	DWORD bytesRead = 0;

	vector<char> readbuffer(filesize);

	BOOL result = ReadFile(fobj.GetHandle(), readbuffer.data(), readbuffer.size(),
		&bytesRead, nullptr);
	_ThrowLastErrorIf(result == FALSE, "ReadFile failed: " + filename);

	cout << filename << " file size: " << filesize << ", bytesRead: "
		<< bytesRead << endl;

	return readbuffer;
}

bool IsFileDiff(const string& filename1, const string& filename2)
{
	return ReadFileVector(filename1) != ReadFileVector(filename2);
}



}
#include <iomanip>  
void testFileIO()
{
	string filename1("file1.txt");
	string filename2("file2.txt");

	try
	{

		cout << "Using file names " << filename1 << " and " << filename2 << endl;

		if (IsFileDiff(filename1, filename2)) {
			cout << "*** Files are different." << endl;
		}
		else {
			cout << "*** Files match." << endl;
		}
	}
	catch (const Win32Exception& e)
	{
		ios state(nullptr);
		state.copyfmt(cout);
		cout << e.what() << endl;
		cout << "Error code: 0x" << hex << uppercase << setw(8) << setfill('0')
			<< e.GetErrorCode() << endl;
		cout.copyfmt(state); // restore previous formatting  
	}

	cout << "done" << endl;
}


