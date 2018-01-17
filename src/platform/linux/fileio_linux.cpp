#include "fileio.h"
#include "exceptions.h"
#include <stb_image.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <string>
#include <limits>
#include <stdexcept>
#include <stdint.h>

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
	strerror_r(error, buf.data(), BUFFERLENGTH);
	if (msg.empty())
		return String(buf.data());
	return String(buf.data()) + "   (" + msg + ")";
}


void ThrowLastErrorIf(bool expression, const String& msg)
{
	if (expression) {
		throw FileIOException(errno, msg);
	}
}

class File
{
private:

	// Declared but not defined, to avoid double closing.
	File& operator=(const File&);
	File(const File&);
public:
	explicit File(const String& filename, int mode)
	{
		//TODO: implement
	}

	~File() {
		//TODO: implement
	}

//	HANDLE GetHandle() { return m_handle; }
};

size_t GetFileSizeSafe(const String& filename)
{
	//TODO: implement
	return 0;
}

int32_t WriteFileVector(const String& filename, vector<uint8_t>& writebuffer)
{
	//TODO: implement
	return 0;
}

void ReadFileVector(const String& filename, vector<uint8_t>& out)
{
	//TODO: implement
}
int promptUserFilePath(int mode, std::vector<SupportedFileType> fileTypes, String& _out) {
	//TODO: implement
	return 0;
}


void findFilesWithExt(
		const String& strPath,
		const String& strExt,
		const bool& bRecursive,
		std::vector<FileFound>& _out, int depth)
{
	//TODO: implement
}
