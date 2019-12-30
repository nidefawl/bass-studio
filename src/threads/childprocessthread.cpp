#include "childprocessthread.h"

#include <vector>
#include <array>
#include <algorithm>
#include "threads.h"
#include "str_util.h"
#include "../host/vst_host.h"
#include "../host/plugin/vst_plugin.h"
#include "fileio.h"
#include "exceptions.h"

#if __linux__
#include <unistd.h>
#include <spawn.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <stdio.h>
class ProcessRunScope {
public:
	int exitCode = 0;
	ProcessRunScope(String binary, String params) {
		pid_t pid;
//		char *argv[] = { "ls", (char *) 0 };
	    std::vector<String> strings;
	    std::istringstream f(params);
	    String s;
	    while (getline(f, s, ' ')) {
	        strings.push_back(s);
	    }
	    const char** argv = (const char**)alloca(sizeof(char*)*(strings.size()+1));
	    for (unsigned i = 0; i < strings.size(); i++) {
	    	argv[i] = StringAsCStr(strings[i]);
	    }
	    argv[strings.size()] = 0;
	    const char* bin = StringAsCStr(binary);
		int status = posix_spawn(&pid, bin, NULL, NULL, (char* const*)argv, environ);
		if (status == 0) {
			int waitRet = waitpid(pid, &status, 0);
			if (waitRet == -1) {
				throw SystemException(errno,
						"Process did not exit normally");
			}
		} else {
			String errmsg = StringFormat("posix_spawn(%s, %s) failed",
					StringAsCStr(binary), StringAsCStr(params));
			throw SystemException(status, errmsg);
		}
	}
	~ProcessRunScope() {

	}
};
#elif defined _WIN32
#include <windows.h>
class ProcessRunScope {
public:
	DWORD exitCode = 0;
	PROCESS_INFORMATION processInformation{0};
	STARTUPINFO startupInfo{0};
	HANDLE handleStdOutRead = NULL;
	HANDLE handleStdOutWrite = NULL;
	ProcessRunScope(const String& binary, const String& params, const String& workingDir, const ProcessThread::Env& env, bool pipeOutput) {
#ifdef UNICODE
#error "not implemented"
#endif
		startupInfo.cb = sizeof(startupInfo);
		std::vector<char> bufEnv;
		if (env.size()) {

			size_t totalEnvLen = 0;
			for (const auto& entry : env) {
				totalEnvLen+=entry.name.length();
				totalEnvLen+=1; // =
				totalEnvLen+=entry.value.length();
				totalEnvLen+=1; // \0
			};
			totalEnvLen+=1; // \0
			bufEnv.resize(totalEnvLen);
			char* dstOffset = bufEnv.data();
			char* dstEnd = bufEnv.data()+totalEnvLen;
			for (const auto& entry : env) {
//				my_printf("ENV[\"%s\"]\t=\t%s\n", StringAsCStr(entry.name), StringAsCStr(entry.value));
				if (strcpy_s(dstOffset, (dstEnd-dstOffset), StringAsCStr(entry.name)))
					throw appexception("Failed processing env key");
				dstOffset += entry.name.length();
				dbgassert((dstEnd-dstOffset) > 0);
				*dstOffset++ = '=';
				if (strcpy_s(dstOffset, (dstEnd-dstOffset), StringAsCStr(entry.value)))
					throw appexception("Failed processing env val");
				dstOffset += entry.value.length();
				dbgassert((dstEnd-dstOffset) > 0);
				*dstOffset++ = '\0';
			}
			dbgassert((dstEnd-dstOffset) == 1);
			*dstOffset++ = '\0';
			dbgassert((dstOffset-bufEnv.data()) == totalEnvLen);
			bufEnv.resize(totalEnvLen);
		} else {

			dbgassert(bufEnv.size()==0);
		}
		std::vector<char> bufWorkingDir;
		if (workingDir.length()) {
			bufWorkingDir.resize(workingDir.length()+1);
			if (strcpy_s(bufWorkingDir.data(), bufWorkingDir.size(), StringAsCStr(workingDir)))
				throw appexception("Failed processing working dir");
		} else {
			dbgassert(bufWorkingDir.size()==0);
		}
		bool inheritHandles = pipeOutput;
		// Create a pipe for the child process's STDOUT.
		if (pipeOutput) {

			SECURITY_ATTRIBUTES saAttr;
			saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
			saAttr.bInheritHandle = TRUE;
			saAttr.lpSecurityDescriptor = NULL;

			if (!CreatePipe(&handleStdOutRead, &handleStdOutWrite, &saAttr, 0))
				throw SystemException(GetLastError(), "CreateProcess CreatePipe failed");

			// Ensure the read handle to the pipe for STDOUT is not inherited.

			if (!SetHandleInformation(handleStdOutRead, HANDLE_FLAG_INHERIT, 0))
				throw SystemException(GetLastError(), "CreateProcess SetHandleInformation failed");

			startupInfo.hStdError = this->handleStdOutWrite;
			startupInfo.hStdOutput = this->handleStdOutWrite;
			startupInfo.dwFlags |= STARTF_USESTDHANDLES;
		}
		if (!CreateProcess(		(LPSTR) StringAsCStr(binary),
								(LPSTR) StringAsCStr(binary+" "+params),
								NULL,
								NULL,
								inheritHandles,
								NORMAL_PRIORITY_CLASS/*|CREATE_NEW_CONSOLE | CREATE_NO_WINDOW*/,
								bufEnv.size()?bufEnv.data():nullptr,
								bufWorkingDir.size()?bufWorkingDir.data():nullptr,
								&startupInfo,
								&processInformation)) {
			String errmsg = StringFormat("CreateProcess(%s, %s) failed", StringAsCStr(binary), StringAsCStr(params));
			throw SystemException(GetLastError(), errmsg);
		}
		if (pipeOutput) {
			CloseHandle(handleStdOutWrite);
			CloseHandle(processInformation.hThread);
		}
	}
	void waitForever() {
		WaitForSingleObject(processInformation.hProcess, INFINITE);
		if (!GetExitCodeProcess(processInformation.hProcess, &exitCode)) {
			throw SystemException(GetLastError(), "Process did not exit normally");
		}
	}
	bool waitTimeOut(int32_t timeoutMsec) {
		dbgassert(timeoutMsec > 0);
		auto ret = WaitForSingleObject(processInformation.hProcess, timeoutMsec);
		if (ret == WAIT_TIMEOUT){
			return false;
		}
		if (ret != WAIT_OBJECT_0) {
			TerminateProcess(processInformation.hProcess, 1);
			throw SystemException(GetLastError(), "WaitForSingleObject returned unexpected state");
		}
		if (!GetExitCodeProcess(processInformation.hProcess, &exitCode)) {
			throw SystemException(GetLastError(), "Process did not exit normally");
		}
		return true;
	}
	~ProcessRunScope() {
		CloseHandle(processInformation.hProcess);
		CloseHandle(handleStdOutRead);
	}
};
#endif
class ProcessThread::Impl
{
	std::thread t;
	std::recursive_mutex mutex;
    std::atomic<bool> isrunning;
    std::atomic<bool> readFailed;
    std::vector<String> buffer;
    bool started = false;
public:
    volatile int32_t processExitCode = 0;
    std::exception_ptr eptr = nullptr;
    String lastCmd = "";
	Impl() {
		isrunning = false;
	}
	~Impl() {
	}
	void flushOutputBuffer(std::vector<char>& buf) {
		while (buf.size() > 0) {
			auto it = std::find_if(buf.begin(), buf.end(), [](const char c) {
				return c == '\n' || c == '\r';
			});

			if (it == buf.begin()) {
				buf.erase(buf.begin(), buf.begin()+1);
				continue;
			}
			if (it == buf.end()) {
				break;
			}
			mutex.lock();
			buffer.emplace_back(buf.begin(), it);
			mutex.unlock();
			if (buf.size())
				it++;
			buf.erase(buf.begin(), it);
		}
	}
	void startProcess(const String& binary, const String& params, const String& workingDir, const Env& env, bool pipedOutput) {
		started = true;
		isrunning = true;
		#ifndef _WIN32
		dbgassert(0&&"Not implemented on this platform");
		#else
		this->lastCmd = StringFormat("%s> %s %s", StringAsCStr(workingDir),  StringAsCStr(binary), StringAsCStr(params));
		t = std::thread([this, argbinary=binary, argparams=params, argwd=workingDir, argenv=env, argpipe=pipedOutput]() {
			setCurrentThreadName("childprocessthread");
			try {
				std::array<char, 2048> TEMP;
				std::vector<char> buf;
				ProcessRunScope scopedProcess(argbinary, argparams, argwd, argenv, argpipe);
				if (argpipe) {
					while (!readFailed) {
						dbgassert(scopedProcess.handleStdOutRead);
						DWORD dwRead = 0;
						if (!ReadFile( scopedProcess.handleStdOutRead, TEMP.data(), TEMP.size(), &dwRead, NULL)) {
							readFailed = true;
							if (scopedProcess.waitTimeOut(50)) {
								break;
							}
						}
						if (dwRead > 0) {
							buf.insert(buf.end(), TEMP.begin(), TEMP.begin()+ dwRead);
							flushOutputBuffer(buf);
						}
						threadSleep(25);
					}
					if (buf.size() > 0) {
						flushOutputBuffer(buf);
					}
				}
				scopedProcess.waitForever();
				processExitCode = (int32_t) scopedProcess.exitCode;
			} catch(...) {
				eptr = std::current_exception();
			}
			log_printf("END OF THREAD\n", 0);
			isrunning = false;
		});
		#endif

	}
	bool isRunning() {
        return isrunning;
	}
	void joinProcess() {
		if (started && t.joinable())
			t.join();
	}
	int32_t readLines(std::vector<String>& out) {
		int32_t size;
		mutex.lock();
		size = buffer.size();
		out = buffer;
		buffer.clear();
		mutex.unlock();
		return size;
	}
};
ProcessThread::ProcessThread() :
	_M_impl { new ProcessThread::Impl {  } } {
}
ProcessThread::~ProcessThread() {
	_M_impl->joinProcess();
	delete _M_impl;
}
void ProcessThread::startProcess(String binary, String params, String workingDir) {
	_M_impl->startProcess(std::move(binary), std::move(params), std::move(workingDir), this->env, this->hasPipedOutput);
}
void ProcessThread::joinProcess() {
	_M_impl->joinProcess();
}
bool ProcessThread::isRunning() {
    return _M_impl->isRunning();
}
bool ProcessThread::checkException() {
	if (_M_impl->eptr != nullptr) {
		try {
			std::rethrow_exception(_M_impl->eptr);
		} catch (const std::exception &ex) {
			printf("process[%s] had exception: %s\n", StringAsCStr(_M_impl->lastCmd), ex.what());
		}
		return true;
	}
	return false;
}
int32_t ProcessThread::readLines(std::vector<String>& out) {
	return _M_impl->readLines(out);
}
int ProcessThread::getExitCode() {
    return _M_impl->processExitCode;
}

void ProcessThread::addEnvPath(const String& path) {
	auto it = std::find_if(env.begin(), env.end(), [](const auto& entry) {
		return entry.name == "Path";
	});
	if (it != env.end()) {
		it->value += ";"+path;
	} else {
		env.push_back({"Path", path});
	}
}
void ProcessThread::addEnv(const String& envKey, const String& envVal) {
	auto it = std::find_if(env.begin(), env.end(), [&envKey](const auto& entry) {
		return entry.name == envKey;
	});
	if (it != env.end()) {
		it->value = envKey;
	} else {
		env.push_back({envKey, envVal});
	}
}
