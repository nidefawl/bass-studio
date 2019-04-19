#include <windows.h>
#include <DbgHelp.h>

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include "logging.h"
#include "str_util.h"

namespace Win32Stacktrace {

	void getStacktraceImpl(std::vector<String>& stackTrace) {

		HANDLE process = GetCurrentProcess();
		HANDLE thread = GetCurrentThread();

		CONTEXT context;
		memset(&context, 0, sizeof(CONTEXT));
		context.ContextFlags = CONTEXT_FULL;
		RtlCaptureContext(&context);

		SymInitialize(process, NULL, TRUE);

		DWORD image;
		STACKFRAME64 frame;
		ZeroMemory(&frame, sizeof(STACKFRAME64));

	#ifdef _M_IX86
		image = IMAGE_FILE_MACHINE_I386;
		frame.AddrPC.Offset = context.Eip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.Ebp;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.Esp;
		frame.AddrStack.Mode = AddrModeFlat;
	#elif _M_X64
		image = IMAGE_FILE_MACHINE_AMD64;
		frame.AddrPC.Offset = context.Rip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.Rsp;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.Rsp;
		frame.AddrStack.Mode = AddrModeFlat;
	#elif _M_IA64
		image = IMAGE_FILE_MACHINE_IA64;
		frame.AddrPC.Offset = context.StIIP;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.IntSp;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrBStore.Offset = context.RsBSP;
		frame.AddrBStore.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.IntSp;
		frame.AddrStack.Mode = AddrModeFlat;
	#endif

		int stackPos = 0;
		char bufPrefix[256];
		char* symbolBuffer = (char*) calloc(1, sizeof(IMAGEHLP_SYMBOL) + (MAX_SYM_NAME+1) * sizeof(TCHAR));
		char* tempBuffer = (char*) calloc(1, (MAX_SYM_NAME+1) * sizeof(TCHAR));

		while (StackWalk64(image, process, thread,
			&frame, &context,
			NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {

			const char* unknownFrame = "???";
			const char* moduleName = unknownFrame;
			const char* functioName = unknownFrame;

			DWORD64 moduleBase = SymGetModuleBase(process, frame.AddrPC.Offset);
			char moduleBuff[MAX_PATH];
			if (moduleBase && GetModuleFileNameA((HINSTANCE)moduleBase, moduleBuff, MAX_PATH))
			{
				moduleName = moduleBuff;
			}

			PSYMBOL_INFO symbol = (PSYMBOL_INFO )symbolBuffer;
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = MAX_SYM_NAME;

			DWORD64 displacement = 0;
			if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
			{
				functioName = symbol->Name;
			}

			DWORD  offset = 0;
			IMAGEHLP_LINE imgline;
			imgline.SizeOfStruct = sizeof(IMAGEHLP_LINE);
			int prefOffset = snprintf(bufPrefix, 128, "%08X ", frame.AddrPC.Offset);
			if (prefOffset < 0) prefOffset = 0;
			if (SymGetLineFromAddr(process, frame.AddrPC.Offset, &offset, &imgline)) {
				const char* shortName = removeLeadingPathSegments(imgline.FileName, 2);
				replaceBackslashWithForwardslash(shortName, tempBuffer, MAX_SYM_NAME+1);
				snprintf(bufPrefix+prefOffset, 128, "%s:%d:%d", tempBuffer, (int)imgline.LineNumber, offset);
			} else {
				snprintf(bufPrefix+prefOffset, 128, "%s:%d", removeLeadingPathSegments(moduleName, 0), (int)displacement);
			}
			stackTrace.push_back(StringFormat("%-42s %s", bufPrefix, functioName));


			stackPos++;

		}
		free(symbolBuffer);
		SymCleanup(process);
	}
}

void logStackTrace() {
	std::vector<String> stackTrace;
	Win32Stacktrace::getStacktraceImpl(stackTrace);
	for (String s : stackTrace) {
		log_out("%s\n", StringAsCStr(s));
	}
}
void getStackTrace(std::vector<String>& vec) {
	Win32Stacktrace::getStacktraceImpl(vec);
}
