#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <excpt.h>
#include "msgbox.h"
#include "winheaders.h"
#include "seq_math.h"
#include "str_util.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <vector>


using std::max;
using std::min;//make code analyzer happy (and make author sad)
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
	wTimerRes = min(max((uint32_t)tc.wPeriodMin, (uint32_t)TARGET_RESOLUTION), (uint32_t)tc.wPeriodMax);
	timeBeginPeriod(wTimerRes);

}

String getLastWin32ErrorString() {

    //Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();
    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    String message(messageBuffer, size);

    //Free the buffer.
    LocalFree(messageBuffer);
    return message;
}

void allocConsole() {
#ifndef __MINGW32__
		AllocConsole();
		AttachConsole(GetCurrentProcessId());
		FILE* f;
		freopen_s(&f, "CON", "w", stdout);
#endif
}

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
#define WINAPI __stdcall
extern String excDescription;
static LONG WINAPI TopLevelExceptionHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
	DWORD excCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
	excDescription = StringFormat("Application crash: %s (0x%08X)", _exc_as_str(excCode), (int)excCode);
//	std::cout << s << std::endl;
	//	ngui::show(StringAsCStr(s), "Error", ngui::Style::Error, ngui::Buttons::OK);
	std::terminate();
//	exit(1);
    return EXCEPTION_EXECUTE_HANDLER;
}
void setExceptionHandler() {

	SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOALIGNMENTFAULTEXCEPT|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(TopLevelExceptionHandler);
}
String getKeyName(int scancode) {
	TCHAR strBuf[512];
	GetKeyNameText(scancode<<16, strBuf, 512);
	return strBuf;
}
