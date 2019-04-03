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
#include <unordered_map>
#include "math/seq_math.h"
#include "fileio.h"
#include "threads.h"
#include "platform.h"

class ThreadSafeFileLogger : public Logger {
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
	void log(const char* data, size_t len) override {
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
	void logStr(String s) override {
		const char* str = StringAsCStr(s);
		log(str, s.length());
	}
};
static ThreadSafeFileLogger& getGlobalLoggerImpl() {
	static ThreadSafeFileLogger gGlobalLogger;
	return gGlobalLogger;
}
Logger* getGlobalLogger() {
	return &getGlobalLoggerImpl();
}
void closeGlobalLog() {
	getGlobalLoggerImpl().logStr("End of logfile\n");
	getGlobalLoggerImpl().closeLog();
}
void openGlobalLog() {
	getGlobalLoggerImpl().openFile("daw.log");
	getGlobalLoggerImpl().logStr("Begin of logfile\n");
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
	char szLogStr[MAX_LEN_MY_PRINTF]{ 0 };
	char szFileShort[MAX_LEN_FILENAME]{ 0 };
	char szLogStatement[MAX_LEN_MY_PRINTF]{ 0 };
	va_list args;
	va_start(args, fmt);
	int ret = vsnprintf(szLogStr, MAX_LEN_MY_PRINTF - 1, fmt, args);
	va_end(args);
	if (ret <= 0) {
		assert(0);
		return;
	}
	sanitizeFilename(szFileShort, relFileName(file));
	String threadName = getCurrentThreadName();
	const char* szThreadName = StringAsCStr(threadName);
//	printf("%s:%d %s: %s", szFileShort, line, func, szLogStr);
	ret = sprintf_s(szLogStatement, MAX_LEN_MY_PRINTF - 1, "%s:%s:%d %s: %s", szThreadName, szFileShort, line, func, szLogStr);
	if (ret > 0) {
		assert(ret+1 <= MAX_LEN_MY_PRINTF);
		fprintf(stdout, szLogStatement);
		getGlobalLoggerImpl().log(szLogStatement, ret);
	}
//	appendLog(szLogStatement);
#ifndef _MSC_VER
	fflush(stdout);
#endif
}
namespace {

struct threadnames_t {
	std::mutex gThreadMutex;
	std::unordered_map<std::thread::id, String> gThreadNames;
	threadnames_t() {
		std::lock_guard<std::mutex> lock(gThreadMutex);
	}
	void setCurrentsName(String str) {
		auto threadId = std::this_thread::get_id();
		std::lock_guard<std::mutex> lock(gThreadMutex);
		gThreadNames[threadId] = str;
	}
	String getCurrentsName() {
		auto threadId = std::this_thread::get_id();
		std::lock_guard<std::mutex> lock(gThreadMutex);
		auto it = gThreadNames.find(threadId);
		if (it == gThreadNames.end()) {
			return StringFormat("thread-%X", static_cast<int32_t>(threadId.get()));
		}
		return it->second+StringFormat("-%X", static_cast<int32_t>(threadId.get()));
	}
};
threadnames_t& getThreadNames() {
	static threadnames_t threadnames;
	return threadnames;
}
}
void setCurrentThreadName(String str) {
	getThreadNames().setCurrentsName(str);
}
String getCurrentThreadName() {
	return getThreadNames().getCurrentsName();
}
void logEveryMsec(int32_t nId, int32_t delayMs, String str) {
	static std::recursive_mutex gLogMutex;
	static std::unordered_map<int32_t, int32_t> gEntries;
	if (str.back() != '\n')
		str += "\n";
	auto now = getTimeMillis();
	bool shouldLog = false;
	{
		std::lock_guard<std::recursive_mutex> lock(gLogMutex);
		auto it = gEntries.find(nId);
		if (it == gEntries.end() || now - it->second > delayMs) {
			shouldLog = true;
			gEntries[nId] = now;
		}
	}
	if (shouldLog) {
		const char* szLogStatement = StringAsCStr(str);
		fprintf(stdout, szLogStatement);
		getGlobalLoggerImpl().log(szLogStatement, str.length());
#ifndef _MSC_VER
		fflush(stdout);
#endif
	}
}
