#include "str_util.h"

#ifdef __GNUC__

#include <cxxabi.h>

using namespace __cxxabiv1;

String demangleName(String to_demangle) {
	int status = 0;
    size_t buff_size = 128;
    auto buff = reinterpret_cast<char*>(std::malloc(buff_size));
	char * buff2 = __cxxabiv1::__cxa_demangle(to_demangle.c_str(), buff, &buff_size, &status);
	String demangled;
	if (buff2) {
		buff = buff2;
		demangled = buff2;
	}
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
#include <assert.h>
#include <vector>
#include "math/seq_math.h"
#include "fileio.h"
#include "threads.h"
#include "platform.h"


class StdOutLogger : public Logger {
public:
	virtual ~StdOutLogger() { }
	void log(const char* data, size_t len) {
	    fwrite(data, len, 1, stdout);
	    fflush(stdout);
	}
	void logStr(String s) {
		fprintf(stdout, "%s", StringAsCStr(s));
	    fflush(stdout);
	}
};
class MultiLogger : public Logger {
	std::vector<Logger*> loggers;
public:
	MultiLogger(Logger* handle) {
		loggers.push_back(handle);
	}
	void addLogger(Logger* _logger) {
		loggers.push_back(_logger);
	}
	void removeLogger(Logger* _logger) {
		loggers.erase(std::remove(loggers.begin(), loggers.end(), _logger), loggers.end());
		loggers.push_back(_logger);
	}
	virtual ~MultiLogger() { }
	void log(const char* data, size_t len) {
		for (auto* logger : loggers) {
			logger->log(data, len);
		}
	}
	void logStr(String s) {
		for (auto* logger : loggers) {
			logger->logStr(s);
		}
	}
};
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
		if (s.back() != '\n')
			s += '\n';
		const char* str = StringAsCStr(s);
		log(str, s.length());
	}
};
static ThreadSafeFileLogger& getFileLogger() {
	static ThreadSafeFileLogger gGlobalLogger;
	return gGlobalLogger;
}
static MultiLogger& getMultiLogger() {
	static MultiLogger gMultiLogger(new StdOutLogger());
	return gMultiLogger;
}
Logger* getGlobalLogger() {
	return &getMultiLogger();
}
void closeGlobalLog() {
	getFileLogger().logStr("End of logfile\n");
	getFileLogger().closeLog();
	getMultiLogger().removeLogger(&getFileLogger());
}
void openGlobalLog() {
	getFileLogger().openFile("daw.log");
	getFileLogger().logStr("Begin of logfile\n");
	getMultiLogger().addLogger(&getFileLogger());
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
void global_log_format_impl(const char *file, int line, const char *func, const char *fmt, ...) {
	char szLogStr[MAX_LEN_MY_PRINTF]{ 0 };
	char szFileShort[MAX_LEN_FILENAME]{ 0 };
	char szLogBuf[MAX_LEN_MY_PRINTF]{ 0 };
	va_list args;
	va_start(args, fmt);
	int ret = vsnprintf(szLogStr, MAX_LEN_MY_PRINTF - 1, fmt, args);
	va_end(args);
	if (ret <= 0) {
		assert(0);
		return;
	}
	const char* szLogStatement = nullptr;
	if (file && line && func) {
		sanitizeFilename(szFileShort, relFileName(file));
		String threadName = getCurrentThreadName();
		const char* szThreadName = StringAsCStr(threadName);
		ret = sprintf_s(szLogBuf, MAX_LEN_MY_PRINTF - 1, "%s:%s:%d %s: %s", szThreadName, szFileShort, line, func, szLogStr);
		if (ret > 0) {
			assert(ret+1 <= MAX_LEN_MY_PRINTF);
			szLogStatement = szLogBuf;
		}
	} else {
		szLogStatement = szLogStr;
	}
	if (szLogStatement) {
		getGlobalLogger()->log(szLogStatement, ret);
	}
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
		getGlobalLogger()->logStr(str);
	}
}
