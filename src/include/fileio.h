#pragma once
#include "str_util.h"
#include <fstream>
#include <vector>
#include <stdint.h>

#ifdef _WIN32
   #include <io.h>
   #define access    _access_s
#else
   #include <unistd.h>
#endif
#include "logging.h"

struct ImageBuf {
	std::vector<uint8_t> bytes;
	int w = 0;
	int h = 0;
	int bitdepth = 0;
};
struct RawFileBuf {
	std::vector<uint8_t> bytes;
	String file;
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

using ByteBuf = std::vector<uint8_t>;

using std::ifstream;
using std::ios;

class window_base;

int32_t WriteFileVector(const String& filename, std::vector<uint8_t>& writebuffer);
void ReadFileVector(const String& filename, std::vector<uint8_t>& out);
int promptUserFilePath(window_base* w, int mode, std::vector<SupportedFileType> fileTypes, String& _out);
void handleGuiEvents();
size_t GetFileSizeSafe(const String& filename);
inline bool FileExists( const String &Filename )
{
    return access( Filename.c_str(), 0 ) == 0;
}
inline int64_t FileSize(const String &fileName)
{
    ifstream file(fileName.c_str(), ifstream::in | ifstream::binary);

    if(!file.is_open())
    {
        return -1;
    }

    file.seekg(0, ios::end);
    size_t fileSize = file.tellg();
    file.close();

    return (int64_t)fileSize;
}

int64_t ReadImage( const String &Filename, ImageBuf& ref);
inline int64_t ReadFileFully( const String &Filename, ByteBuf& ref)
{
    if (FileExists(Filename)) {
    	int64_t size = FileSize(Filename);
    	if (size > 0) {
    	    ifstream file(Filename.c_str(), ifstream::in | ifstream::binary);
    		if (file) {
    			ref.reserve(size);
    			ref.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    		}
    		size_t cur = file.tellg();
    		if (size != (int64_t)cur) {
    			my_printf("read %d bytes, expected %d bytes, BAD!\n", cur, size);
    		} else {

        	    return size;
    		}

    	}
    }
    return -1;
}

inline void SplitPath(String in, String* path, String* name, String* ext, String* nameExt = NULL) {
     std::size_t pathSep = in.find_last_of("/\\");
     String _nameExt;
	 if (pathSep == String::npos) {
		 _nameExt = in;
	 } else {
		 _nameExt = pathSep+1 < in.length() ? in.substr(pathSep+1) : "";
	 }
     if (path != NULL) {
    	 if (pathSep == String::npos) {
    		 *path = "";
    	 } else {
    		 *path = in.substr(0, pathSep);
    	 }
     }
     std::size_t fileExtSep = _nameExt.find_last_of(".");
     if (name != NULL) {
    	 if (fileExtSep == String::npos || fileExtSep < 1) {
    		 *name = _nameExt;
    	 } else {
    		 *name = _nameExt.substr(0, fileExtSep);
    	 }
     }
     if (ext != NULL) {
    	 if (fileExtSep == String::npos || fileExtSep >= _nameExt.length()-1) {
    		 *ext = "";
    	 } else {
    		 *ext = _nameExt.substr(fileExtSep+1);
    	 }
     }
     if (nameExt != NULL) {
    	 *nameExt = _nameExt;
     }
}
inline String FileNameFromPath(String in) {
	String fileName;
	SplitPath(in, NULL, NULL, NULL, &fileName);
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
	FileTimeGetter(String path);
	~FileTimeGetter();
private:
	Impl* _M_Impl;
};
