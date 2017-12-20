#include "childprocessthread.h"
#ifdef __MINGW32__
#undef _GLIBCXX_HAS_GTHREADS
#include "mingw.thread.h"
#include <mutex>
#include "mingw.mutex.h"
#include "mingw.condition_variable.h"
#else
#include <mutex>
#endif

#include "str_util.h"
#include "../host/vst_host.h"
#include "../host/vst_plugin.h"
#include "fileio.h"
#include "exceptions.h"
#include <windows.h>
class ProcessThread::Impl
{
	std::thread t;
    std::atomic<bool> isrunning;
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
	class ProcessRunScope {
	public:
	    DWORD exitCode = 0;
		PROCESS_INFORMATION processInformation = { 0 };
		STARTUPINFO startupInfo = { 0 };
		ProcessRunScope(String binary, String params) {
			startupInfo.cb = sizeof(startupInfo);
			if (CreateProcess((LPSTR) StringAsCStr(binary), (LPSTR) StringAsCStr(params),
									NULL, NULL, FALSE,
									NORMAL_PRIORITY_CLASS|CREATE_NEW_CONSOLE /*| CREATE_NO_WINDOW*/,
									NULL, NULL, &startupInfo, &processInformation)) {
				WaitForSingleObject(processInformation.hProcess, INFINITE);
//				if (WAIT_OBJECT_0 != WaitForSingleObject(processInformation.hProcess, 30000)) {
//					TerminateProcess(processInformation.hProcess, 1);
//				}
				if (!GetExitCodeProcess(processInformation.hProcess, &exitCode)) {
					throw SystemException(GetLastError(), "Process did not exit normally");
				}
			} else {
				String errmsg = StringFormat("CreateProcess(%s, %s) failed", StringAsCStr(binary), StringAsCStr(params));
				throw SystemException(GetLastError(), errmsg);
			}
		}
		~ProcessRunScope() {
			CloseHandle(processInformation.hProcess);
			CloseHandle(processInformation.hThread);
		}
	};
	void startProcess(String& binary, String& params) {
		started = true;
		isrunning = true;
		this->lastCmd = StringFormat("%s %s", StringAsCStr(binary), StringAsCStr(params));
		t = std::thread([this, binary, params]() {
			try {
				ProcessRunScope scopedProcess(binary, params);
				processExitCode = (int32_t) scopedProcess.exitCode;
			} catch(...) {
				eptr = std::current_exception();
			}
			isrunning = false;
		});

	}
	bool isRunning() {
        return isrunning;
	}
	void joinProcess() {
		if (started)
			t.join();
	}
};
ProcessThread::ProcessThread() :
	_M_impl { new ProcessThread::Impl {  } } {
}
ProcessThread::~ProcessThread() {

}
void ProcessThread::startProcess(String binary, String params) {
	_M_impl->startProcess(binary, params);
}
void ProcessThread::joinProcess() {
	_M_impl->joinProcess();
}
bool ProcessThread::isRunning() {
    return _M_impl->isRunning();
}
bool ProcessThread::checkExcepetion() {
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
int ProcessThread::getExitCode() {
    return _M_impl->processExitCode;
}

