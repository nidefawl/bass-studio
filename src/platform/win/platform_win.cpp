#ifdef _WIN32
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <excpt.h>
#include <assert.h>
#include "msgbox.h"
#include <Windows.h>
#ifdef __MINGW32__
#include "mmsystem.h"
#else
#include <timeapi.h>
#endif
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#ifdef __MINGW32__
#undef _GLIBCXX_HAS_GTHREADS
#include "../platform/mingw/mingw.thread.h"
#include <mutex>
#include "../platform/mingw/mingw.mutex.h"
#include "../platform/mingw/mingw.condition_variable.h"
#else
#include <mutex>
#endif
#include "math/seq_math.h"
#include "str_util.h"
#include "logging.h"
#include "error.h"

uint64_t getTimeMillis() {
	return (uint64_t) timeGetTime();
}

double getTimeHPC()
{
  static LARGE_INTEGER frequency;
  if (frequency.QuadPart == 0)
	::QueryPerformanceFrequency(&frequency);
  LARGE_INTEGER now;
  ::QueryPerformanceCounter(&now);
  return now.QuadPart / double(frequency.QuadPart);
}
int64_t getTimeHPint64()
 {
	static LARGE_INTEGER frequency;
	if (frequency.QuadPart == 0)
		::QueryPerformanceFrequency(&frequency);
	LARGE_INTEGER now;
	::QueryPerformanceCounter(&now);
//	now.QuadPart *= 1000000UL; //microseconds resolution

	//prevent overflow, but keep some precision
	now.QuadPart *= 10000UL;
	int64_t val = now.QuadPart / frequency.QuadPart;
	assert(val > 0);
	val *= 100UL;
    return val;
}
double getSince(double& d) //checks for overflow
{
	double now = getTimeHPC();
	if (now < d) d = now;
	return now - d;
}
String getModuleName(HMODULE module) {
	std::vector<TCHAR> pathBuf;
	DWORD copied = 0;
	do {
	    pathBuf.resize(pathBuf.size()+MAX_PATH);
	    copied = GetModuleFileName(module, &pathBuf.at(0), pathBuf.size());
	} while( copied >= pathBuf.size() );

	pathBuf.resize(copied);

	String path(pathBuf.begin(),pathBuf.end());
	return path;
}
void setMinimumResolutionTimer() {
#define TARGET_RESOLUTION 1u         // 1-millisecond target resolution

	TIMECAPS tc;
	UINT     wTimerRes;

	if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
	{
		// Error; application can't continue.
	}
	wTimerRes = math::min(math::max((uint32_t)tc.wPeriodMin, (uint32_t)TARGET_RESOLUTION), (uint32_t)tc.wPeriodMax);
	timeBeginPeriod(wTimerRes);

}


void allocConsole() {
#ifndef __MINGW32__
		AllocConsole();
		AttachConsole(GetCurrentProcessId());
		FILE* f;
		freopen_s(&f, "CON", "w", stdout);
#endif
}
#ifdef USE_WIN32_EXC_HOOKS
static const char* _exc_as_str(DWORD excCode) {
	switch (excCode) {
	case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
	case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
	case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
	case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";
	case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
	case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
	case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
	case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
	case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
	case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
	case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION";
	case EXCEPTION_GUARD_PAGE: return "EXCEPTION_GUARD_PAGE";
	case EXCEPTION_INVALID_HANDLE: return "EXCEPTION_INVALID_HANDLE";
	}
	return "UKNOWN_EXCEPTION";
}
static int toErrorCode(DWORD excCode) {
	switch (excCode) {
	case EXCEPTION_ACCESS_VIOLATION: return ERR_ACCESSVIOLATION;
//	case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
//	case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
//	case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP";
//	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
//	case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
//	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
//	case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";
//	case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
//	case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
//	case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
//	case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
//	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
//	case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
//	case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
//	case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
//	case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
//	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
//	case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
//	case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION";
//	case EXCEPTION_GUARD_PAGE: return "EXCEPTION_GUARD_PAGE";
//	case EXCEPTION_INVALID_HANDLE: return "EXCEPTION_INVALID_HANDLE";
	default:
		break;
	}
	return ERR_UNKNOWN;
}
#define WINAPI __stdcall
static LONG WINAPI TopLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
	DWORD excCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
	if (handleFatalError(toErrorCode(excCode), static_cast<int32_t>(excCode))) {
		return EXCEPTION_CONTINUE_EXECUTION;
	}
	String excDescription = StringFormat("Application crash: %s (0x%08X)", _exc_as_str(excCode), (int)excCode);
	my_printf("Fatal: %s\n", StringAsCStr(excDescription));
	std::terminate();
    return EXCEPTION_EXECUTE_HANDLER;
}
#if defined(_MSC_VER)
int __cdecl DebugReportHook(int nReportType, char*, int* pnRet)
{
	int ret = nReportType == _CRT_ASSERT || nReportType == _CRT_ERROR;
	*pnRet = ret;
	return ret ? TRUE : FALSE;
}
#endif
void setExceptionHandler() {
#if defined(_MSC_VER) || (defined(__MSVCRT_VERSION__) && __MSVCRT_VERSION__ > 0x800)
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#if defined(_MSC_VER)
	//this is here to trigger a breakpoint when assert(0) is called using the ms c-runtime
	//by default ms crt throws an exception on assert(0) and opens a dialog that interferes with out wndProc
	_CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, DebugReportHook);
#endif
	SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOALIGNMENTFAULTEXCEPT|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(TopLevelExceptionHandler);
}
#endif
String getKeyName(int scancode) {
	TCHAR strBuf[512];
	GetKeyNameText(scancode<<16, strBuf, 512);
	return strBuf;
}
void threadSleep(int millis) {

	std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}
String FormatErrorMessage(int32_t error, String msg)
{
	static const int BUFFERLENGTH = 1024;
	std::vector<char> buf(BUFFERLENGTH);
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, error, 0, buf.data(),
		BUFFERLENGTH - 1, 0);
	if (msg.empty())
		return String(buf.data());
	return msg + " (" + StringTrim(String(buf.data())) + ")";
}
namespace seqthreads {
int32_t currentThreadsId() {
	return static_cast<int32_t>(std::this_thread::get_id().get());
}
}

#endif
