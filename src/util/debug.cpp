#include "str_util.h"

#ifdef __GNUC__

#include <cxxabi.h>

using namespace __cxxabiv1;

String demangleName(String to_demangle) {
	int status = 0;
	char * buff = __cxxabiv1::__cxa_demangle(to_demangle.c_str(), NULL, NULL, &status);
	String demangled = buff;
	std::free(buff);
	return demangled;
}

#else
String demangleName(String to_demangle)
{
	return to_demangle;
}
#endif
#include "logging.h"
#include <assert.h>
#include <ctime>
#include "math/seq_math.h"
#include "fileio.h"
#include "threads.h"
class ThreadSafeFileLogger {
	std::thread daemon;
	std::recursive_mutex mutex;
	IOFile* handle = nullptr;
public:
	ThreadSafeFileLogger() {
	}
	~ThreadSafeFileLogger() {
		if (handle) {
			delete handle;
		}
	}
	//Not threadsafe
	void openFile(const String& filename) {
		closeLog();
		handle = IOFile::openFile(filename, OpenFileMode::READWRITE);
		if (handle && !handle->isValid()) {
			closeLog();
		}
	}
	//Not threadsafe
	void closeLog() {
		if (handle) {
			delete handle;
			handle = nullptr;
		}
	}

public:
	//threadsafe
	void log(const char* data, size_t len) {
		if (handle) {
			std::lock_guard<std::recursive_mutex> lockguard(mutex);
		    std::time_t t = std::time(nullptr);
		    char mbstr[100];
		    size_t posDateTime = std::strftime(mbstr, sizeof(mbstr), "%Y-%m-%dT%H:%M:%S ", std::localtime(&t));
		    if (posDateTime > 0) {
				handle->write(mbstr, posDateTime);
		    }
			handle->write(data, len);
			handle->flush();
			if (!handle->isValid()) {
				closeLog();
			}
		}
	}
	void logStr(String s) {
		const char* str = StringAsCStr(s);
		log(str, s.length());
	}
};
ThreadSafeFileLogger& getGlobalLogger() {
	static ThreadSafeFileLogger gGlobalLogger;
	return gGlobalLogger;
}
void closeGlobalLog() {
	getGlobalLogger().logStr("End of logfile\n");
	getGlobalLogger().closeLog();
}
void openGlobalLog() {
	getGlobalLogger().openFile("daw.log");
	getGlobalLogger().logStr("Begin of logfile\n");
}
#define MAX_LEN_MY_PRINTF 4096
#define MAX_LEN_FILENAME 512
/* finds the path segment /src/ by reverse search on input
 * then returns everything after /src/
 *  C:\Users\Michael\daw\src\host\vst_host.cpp -> \host\vst_host.cpp
 * */
inline const char* relFileName(const char* input) {
	if (input) {
		size_t inLen = strlen(input);
		const char* pos = input+inLen;
		while (pos >= input) {
			if (*pos == '\\' || *pos == '/') {
				const char* pos2 = pos-1;
				while (pos2 >= input) {
					if (*pos2 == '\\' || *pos2 == '/') {
						if (!strncmp(pos2+1, "src", 3))
							return math::min(input+inLen-1, pos+1);
						break;
					}
					pos2--;
				}
			}
			pos--;
		}
	}
	return input;
}
void sanitizeFilename(char* buf, const char* filename) {
	size_t inLen = strlen(filename);
	assert(inLen+1 < MAX_LEN_FILENAME);
	char* out = &buf[0];
	const char* in = &filename[0];
	size_t i = 0;
	for (; i < inLen; i++) {
		*out++ = (*in == '\\') ? '/' : *in;
		in++;
	}
	buf[i] = '\0';
}
void _my_printf(const char *file, int line, const char *func, const char *fmt, ...) {
	char buf[MAX_LEN_MY_PRINTF];
	char fnamebuf[MAX_LEN_FILENAME];
	va_list args;
	va_start(args, fmt);
	int ret = vsnprintf(buf, MAX_LEN_MY_PRINTF - 1, fmt, args);
	va_end(args);
	if (ret <= 0) {
		assert(0);
		return;
	}
	sanitizeFilename(fnamebuf, relFileName(file));
	printf("%s:%d %s: %s", fnamebuf, line, func, buf);
	char buf2[MAX_LEN_MY_PRINTF] = { 0 };
	ret = sprintf_s(buf2, MAX_LEN_MY_PRINTF - 1, "%s:%d %s: %s", fnamebuf, line, func, buf);
	if (ret > 0) {
		assert(ret+1 <= MAX_LEN_MY_PRINTF);
		getGlobalLogger().log(buf2, ret);
	}
//	appendLog(buf2);
#ifndef _MSC_VER
	fflush(stdout);
#endif
}
